#include "FFGLTemplate.h"
using namespace ffglex;
using namespace ffglqs;

static CFFGLPluginInfo PluginInfo(
	PluginFactory< FFGLTemplate >,	// Create method
	"FF01",							// Plugin unique ID of maximum length 4.
	"FFGL Template",				// Plugin name
	2,								// API major version number
	1,								// API minor version number
	1,								// Plugin major version number
	0,								// Plugin minor version number
	FF_SOURCE,						// Plugin type
	"Copy me",						// Plugin description
	"FFGL Template"					// About
);

static const char vertexShader[] = R"(
#version 410 core

layout( location = 0 ) in vec4 vPosition;
layout( location = 1 ) in vec2 vUV;

out vec2 uv;

void main()
{
	gl_Position = vPosition;
	uv = vUV;
}
)";

static const char fragmentShader[] = R"(
in vec2 uv;

void main()
{
	//Render a frame
	fragColor = vec4(1.0);
}
)";

FFGLTemplate::FFGLTemplate()
{
	// Input properties
	SetMinInputs( 0 );
	SetMaxInputs( 0 );
}

FFGLTemplate::~FFGLTemplate()
{
}

FFResult FFGLTemplate::InitGL( const FFGLViewportStruct* vp )
{
	std::string fragmentShaderCode = CreateFragmentShader( fragmentShaderBase );

	vertexShaderCode = vertexShader;
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

	//Use base-class init as success result so that it retains the viewport.
	return CFFGLPlugin::InitGL( vp );
}

FFResult FFGLTemplate::DeInitGL()
{
	shader.FreeGLResources();
	quad.Release();

	return FF_SUCCESS;
}

FFResult FFGLTemplate::ProcessOpenGL( ProcessOpenGLStruct* inputTextures )
{
	UpdateAudioAndTime();

	//Scoped shader binding:
	//binds the shader program and release it at the end of this method
	ScopedShaderBinding shaderBinding( shader.GetGLID() );

	//This takes care of sending all the parameter that the plugin registered to the shader.
	SendDefaultParams( shader );
	SendParams( shader );

	return Render( inputTextures );
}

FFResult FFGLTemplate::Render( ProcessOpenGLStruct* inputTextures )
{
	quad.Draw();

	return FF_SUCCESS;
}
