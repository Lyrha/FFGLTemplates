#include "MorphFFGL.h"

using namespace ffglex;
using namespace ffglqs;

ffglqs::MorphFFGL::MorphFFGL( 
	unsigned int minInputs, 
	unsigned int maxInputs, 
	std::string vertShader, 
	std::string fragShader ) :
	vertexShader(vertShader),
	fragmentShader(fragShader),
	passes( 0 ),
	passIndex( 0 ),
	isMultipass( false )
{
	// Input properties
	SetMinInputs( minInputs );
	SetMaxInputs( maxInputs );

	//Time is handled by FFGL
	SetTimeSupported( false );

	fragmentShaderAudioSnippet = R"(

	//Common frequency ranges we can use 
	vec2 subBassRange = vec2(20,60);
	vec2 bassRange = vec2(60,250);
	vec2 lowMidRange = vec2(250,500);
	vec2 midRange = vec2(500,2000);
	vec2 highMidRange = vec2(2000,4000);
	vec2 trebleRange = vec2(4000,6000);
	vec2 highTrebleRange = vec2(6000,20000);

	uniform float sampleRate;

	uniform sampler2D MORPH_RAW_AUDIO;
	uniform sampler2D MORPH_FFT_AUDIO;

	float MORPH_GET_RAW_AUDIO(int channel, int bin)
	{
		return texelFetch(MORPH_RAW_AUDIO,ivec2(bin,channel),0).r;
	}

	int MORPH_GET_FFT_AUDIO_FREQ_BIN(float freq)
	{
		float fftSize = textureSize(MORPH_FFT_AUDIO,0).x;
		float HzPerBin = (sampleRate/4.0)/fftSize;

		int bin = int(floor(freq/HzPerBin));

		return isinf(bin)?0:bin;
	}

	float MORPH_GET_FFT_AUDIO(int channel, int bin)
	{
		return texelFetch(MORPH_FFT_AUDIO,ivec2(bin,channel),0).r;
	}

	float MORPH_GET_FFT_AUDIO_FREQ(int channel, float freq)
	{
		int bin = MORPH_GET_FFT_AUDIO_FREQ_BIN(freq);
		return MORPH_GET_FFT_AUDIO(channel,bin);
	}

	float MORPH_GET_FFT_AUDIO_FREQ(int channel, float freqStart, float freqEnd)
	{
		int binStart = MORPH_GET_FFT_AUDIO_FREQ_BIN(freqStart);
		int binEnd = MORPH_GET_FFT_AUDIO_FREQ_BIN(freqEnd);

		int numBins = binStart - binEnd;			

		float freqAverage = 0.0;
		for(int i = binStart; i < binEnd; i++)
			freqAverage += MORPH_GET_FFT_AUDIO(channel,i);
	
		freqAverage /= float(numBins);

		return (numBins==0)?0.0:freqAverage; 
	}

	float MORPH_GET_FFT_AUDIO_FREQ(int channel, vec2 freqRange)
	{
		return MORPH_GET_FFT_AUDIO_FREQ(channel,freqRange.s,freqRange.t);
	}
	)";
}

MorphFFGL::MorphFFGL( 
	unsigned int minInputs, 
	unsigned int maxInputs, 
	std::string vertShader, 
	std::string fragShader, 
	std::vector< std::string > bufferNames ) :
	MorphFFGL( 
		minInputs, 
		maxInputs,
		vertShader,
		fragShader)
{
	passes = bufferNames.size();
	isMultipass = true;
	for( std::string bufferName : bufferNames )
	{
		ffglex::FFGLFBO main;
		ffglex::FFGLFBO backup;
		multipassFBOs.push_back( persistentFBO{ bufferName, false, main, backup } );
	}
}

FFResult MorphFFGL::InitGL( const FFGLViewportStruct* viewport )
{ 
	std::string fragmentShaderCode = CreateFragmentShader( fragmentShaderBase );

	vertexShaderCode = vertexShader;
	fragmentShaderCode += fragmentShaderAudioSnippet;
	fragmentShaderCode += fragmentShader;

	if( !shader.Compile( vertexShaderCode, fragmentShaderCode ) )
	{
		DeInitGL();
		return FF_FAIL;
	}
	if( !quad.Initialise() )
	{
		DeInitGL();
		return FF_FAIL;
	}
	if( Init() == FF_FAIL )
	{
		Log( "Init failed" );
		DeInitGL();
		return FF_FAIL;
	}

	return CFFGLPlugin::InitGL( viewport );
}

FFResult MorphFFGL::DeInitGL()
{
	shader.FreeGLResources();
	quad.Release();
	Clean();
	return FF_SUCCESS;
}

FFResult MorphFFGL::Init()
{ /*
	if( isMultipass )
		if( !initializeFBOs() )
			return FF_FAIL;
			*/
	return FF_SUCCESS;
}

void MorphFFGL::Clean()
{
	if( isMultipass )
		deInitializeFBOs();
}

FFResult MorphFFGL::ProcessOpenGL( ProcessOpenGLStruct* inputTextures )
{
	UpdateAudioAndTime();
	//Activate our shader using the scoped binding so that we'll restore the context state when we're done.
	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	SendDefaultParams( shader );
	SendParams( shader );
	//Send our GPU audio parameters
	SendSampleRate( shader );
	Update();
	FFResult result = Render( inputTextures );
	consumeAllTrigger();

	return result;
}

FFResult MorphFFGL::Render( ProcessOpenGLStruct* inputTextures )
{
	//Activate our shader using the scoped binding so that we'll restore the context state when we're done.
	ScopedShaderBinding shaderBinding( shader.GetGLID() );

	//Multipass
	if (isMultipass)
	{
		//Do multipass rendering first
		RenderMultipass( inputTextures );

		//Render the last pass again to the host FBO
		bindMultipassTextures();
		quad.Draw();
		unbindMultipassTextures();
	}
	else
	{
		quad.Draw();
	}

	return FF_SUCCESS;
}

void MorphFFGL::SendSampleRate( ffglex::FFGLShader& shader )
{
	//Set audio sample rate as a uniform in our fragment shader
	shader.Set( "sampleRate", static_cast<float>(sampleRate) );
}

FFResult MorphFFGL::RenderMultipass( ProcessOpenGLStruct* inputTextures )
{
	//Render to our internal render FBOs with the exact same setup
	//With this setup we are one frame behind but this is cleaner 
	for( passIndex = 0; passIndex < passes; passIndex++ )
	{
		if( !initializeFBO( passIndex ) )
			return FF_FAIL;

		if( !resizeFBO( passIndex ) )
			return FF_FAIL;

		shader.Set( "PASSINDEX", static_cast< int >( passIndex ) );

		persistentFBO& currentFBO = multipassFBOs[ passIndex ];
		bool evenFrame            = ( frame % 2 == 0 );

		ScopedFBOBinding fboBinding( evenFrame ? currentFBO.main.GetGLID() : currentFBO.backup.GetGLID(), ScopedFBOBinding::RestoreBehaviour::RB_REVERT );

		bindMultipassTextures();

		//Render output for the current pass
		quad.Draw();

		unbindMultipassTextures();
	}

	return FF_SUCCESS;
}

bool MorphFFGL::initializeFBOs()
{
	for( unsigned int i = 0; i < passes; i++ )
		if( !initializeFBO( i ) )
			return false;

	return true;
}

bool MorphFFGL::deInitializeFBOs()
{
	for( unsigned int i = 0; i < passes; i++ )
		if( !deInitializeFBO( i ) )
			return false;

	return true;
}

bool MorphFFGL::resizeFBOs()
{
	for( unsigned int i = 0; i < passes; i++ )
		if( !resizeFBO( i ) )
			return false;

	return true;
}

bool MorphFFGL::initializeFBO( unsigned int index )
{
	if( multipassFBOs[ index ].initialized )
		return true;

	multipassFBOs[ index ].initialized = ( multipassFBOs[ index ].main.Initialise( currentViewport.width, currentViewport.height, GL_RGBA8 ) &&
										   multipassFBOs[ index ].backup.Initialise( currentViewport.width, currentViewport.height, GL_RGBA8 ) );

	return multipassFBOs[ index ].initialized;
}

bool MorphFFGL::deInitializeFBO( unsigned int index )
{
	multipassFBOs[ index ].main.Release();
	multipassFBOs[ index ].backup.Release();

	multipassFBOs[ index ].initialized = false;

	return true;
}

bool MorphFFGL::resizeFBO( unsigned int index )
{
	GLuint currentFBOWidth  = multipassFBOs[ index ].main.GetWidth();
	GLuint currentFBOHeight = multipassFBOs[ index ].main.GetHeight();

	if( currentFBOWidth != currentViewport.width || currentFBOHeight != currentViewport.height )
	{
		if( !deInitializeFBO( index ) )
			return false;
		if( !initializeFBO( index ) )
			return false;
	}

	return true;
}

void MorphFFGL::bindMultipassTextures()
{
	bool evenFrame = ( frame % 2 == 0 );

	for( unsigned int i = 0; i < passes; i++ )
	{
		glActiveTexture( GL_TEXTURE0 + i );
		glBindTexture( GL_TEXTURE_2D, evenFrame ? multipassFBOs[ i ].backup.GetTextureInfo().Handle : multipassFBOs[ i ].main.GetTextureInfo().Handle );

		shader.Set( multipassFBOs[ i ].name.c_str(), static_cast< int >( i ) );
	}
}

void MorphFFGL::unbindMultipassTextures()
{
	for( unsigned int i = 0; i < passes; i++ )
	{
		glActiveTexture( GL_TEXTURE0 + i );
		glBindTexture( GL_TEXTURE_2D, 0 );
	}
}