#include "MorphFFGLSource.h"

using namespace ffglex;
using namespace ffglqs;

MorphFFGLSource::MorphFFGLSource( 
	std::string vertShader, 
	std::string fragShader ) :
	MorphFFGL(0,2,vertShader,fragShader)
{
}

MorphFFGLSource::MorphFFGLSource( std::string vertShader, std::string fragShader, std::vector< std::string > bufferNames ) :
	MorphFFGL( 0, 2, vertShader, fragShader, bufferNames )
{
}

FFResult MorphFFGLSource::Render( ProcessOpenGLStruct* inputTextures )
{
	//Activate our shader using the scoped binding so that we'll restore the context state when we're done.
	ScopedShaderBinding shaderBinding( shader.GetGLID() );

	GLuint rawAudioHandle = 0;
	GLuint FFTAudioHandle = 0;

	//Check if the host has provided valid raw audio or FFT textures
	if( inputTextures->inputTextures != nullptr )
	{
		rawAudioHandle = ( inputTextures->inputTextures[ 0 ] != nullptr ) ? ( inputTextures->inputTextures[ 0 ]->Handle ) : ( rawAudioHandle );
		FFTAudioHandle = ( inputTextures->inputTextures[ 1 ] != nullptr ) ? ( inputTextures->inputTextures[ 1 ]->Handle ) : ( FFTAudioHandle );
	}

	ScopedSamplerActivation activateSampler0( 0 );
	Scoped2DTextureBinding textureBinding0( rawAudioHandle );
	ScopedSamplerActivation activateSampler1( 1 );
	Scoped2DTextureBinding textureBinding1( FFTAudioHandle );

	shader.Set( "MORPH_RAW_AUDIO", 0 );
	shader.Set( "MORPH_FFT_AUDIO", 1 );
	
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

void MorphFFGLSource::bindMultipassTextures()
{
	bool evenFrame = ( frame % 2 == 0 );

	for( unsigned int i = 0; i < passes; i++ )
	{
		glActiveTexture( GL_TEXTURE0 + ( i + 2 ) );
		glBindTexture( GL_TEXTURE_2D, evenFrame ? multipassFBOs[ i ].backup.GetTextureInfo().Handle : multipassFBOs[ i ].main.GetTextureInfo().Handle );

		shader.Set( multipassFBOs[ i ].name.c_str(), static_cast< int >( i + 2 ) );
	}
}

void MorphFFGLSource::unbindMultipassTextures()
{
	for( unsigned int i = 0; i < passes; i++ )
	{
		glActiveTexture( GL_TEXTURE0 + ( i + 2 ) );
		glBindTexture( GL_TEXTURE_2D, 0 );
	}
}