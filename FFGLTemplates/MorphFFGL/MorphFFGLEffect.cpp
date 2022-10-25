#include "MorphFFGLEffect.h"

using namespace ffglex;
using namespace ffglqs;

static std::string effectFragShaderSnippet =  R"(
	uniform sampler2D inputTexture;
	)";

MorphFFGLEffect::MorphFFGLEffect( 
	std::string vertShader, 
	std::string fragShader ) :
	MorphFFGL(1,3,vertShader,effectFragShaderSnippet+fragShader)
{
}

MorphFFGLEffect::MorphFFGLEffect( std::string vertShader, std::string fragShader, std::vector< std::string > bufferNames ) :
	MorphFFGL(1,3,vertShader,effectFragShaderSnippet+fragShader,bufferNames)
{
}

FFResult MorphFFGLEffect::Render( ProcessOpenGLStruct* inputTextures )
{
	//Activate our shader using the scoped binding so that we'll restore the context state when we're done.
	ScopedShaderBinding shaderBinding( shader.GetGLID() );

	GLuint inputHandle    = 0;
	GLuint rawAudioHandle = 0;
	GLuint FFTAudioHandle = 0;

	//Check if the host has provided valid raw audio or FFT textures as well as a valid input texture
	if( inputTextures->inputTextures != nullptr )
	{
		inputHandle    = ( inputTextures->inputTextures[ 0 ] != nullptr ) ? ( inputTextures->inputTextures[ 0 ]->Handle ) : ( inputHandle );
		rawAudioHandle = ( inputTextures->inputTextures[ 1 ] != nullptr ) ? ( inputTextures->inputTextures[ 1 ]->Handle ) : ( rawAudioHandle );
		FFTAudioHandle = ( inputTextures->inputTextures[ 2 ] != nullptr ) ? ( inputTextures->inputTextures[ 2 ]->Handle ) : ( FFTAudioHandle );
	}

	ScopedSamplerActivation activateSampler0( 0 );
	Scoped2DTextureBinding textureBinding0( inputHandle );
	ScopedSamplerActivation activateSampler1( 1 );
	Scoped2DTextureBinding textureBinding1( rawAudioHandle );
	ScopedSamplerActivation activateSampler2( 2 );
	Scoped2DTextureBinding textureBinding2( FFTAudioHandle );

	shader.Set( "inputTexture", 0 );
	shader.Set( "MORPH_RAW_AUDIO", 1 );
	shader.Set( "MORPH_FFT_AUDIO", 2 );

	//Multipass
	if( isMultipass )
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

void MorphFFGLEffect::bindMultipassTextures()
{
	bool evenFrame = ( frame % 2 == 0 );

	for( unsigned int i = 0; i < passes; i++ )
	{
		glActiveTexture( GL_TEXTURE0 + ( i + 3 ) );
		glBindTexture( GL_TEXTURE_2D, evenFrame ? multipassFBOs[ i ].backup.GetTextureInfo().Handle : multipassFBOs[ i ].main.GetTextureInfo().Handle );

		shader.Set( multipassFBOs[ i ].name.c_str(), static_cast< int >( i + 3 ) );
	}
}

void MorphFFGLEffect::unbindMultipassTextures()
{
	for( unsigned int i = 0; i < passes; i++ )
	{
		glActiveTexture( GL_TEXTURE0 + ( i + 3 ) );
		glBindTexture( GL_TEXTURE_2D, 0 );
	}
}