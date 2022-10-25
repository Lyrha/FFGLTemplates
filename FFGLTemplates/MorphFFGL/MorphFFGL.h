#pragma once
#include <FFGLSDK.h>

#include <ffglex/FFGLScopedFBOBinding.h>

namespace ffglqs
{

class MorphFFGL : public Plugin
{
public:
	//A basic struct used for multi-pass rendering
	typedef struct persistentFBO
	{
		std::string name;

		bool initialized;

		ffglex::FFGLFBO main;
		ffglex::FFGLFBO backup;
	} 
	persistentFBO;

	//Call this constructor for single-pass plugins
	MorphFFGL(
		unsigned int minInputs,
		unsigned int maxInputs,
		std::string vertShader,
		std::string fragShader);

	//Call this constructor for multi-pass plugins
	MorphFFGL(
		unsigned int minInputs,
		unsigned int maxInputs,
		std::string vertShader,
		std::string fragShader,
		std::vector< std::string > bufferNames );

	virtual FFResult InitGL( const FFGLViewportStruct* viewport ) override;
	virtual FFResult DeInitGL() override;
	virtual FFResult Init() override;
	virtual void Clean() override;

	virtual FFResult ProcessOpenGL( ProcessOpenGLStruct* inputTextures ) override;

	virtual FFResult Render( ProcessOpenGLStruct* inputTextures ) override;
	
	//Sample rate is not sent to the shader by default; call this method in ProcessOpenGL
	virtual void SendSampleRate( ffglex::FFGLShader& shader );

	template< typename PluginType >
	static PluginInstance CreatePlugin( PluginInfo infos );

protected:
	//Multi-pass methods
	virtual FFResult RenderMultipass( ProcessOpenGLStruct* inputTextures );

	virtual bool initializeFBOs();
	virtual bool deInitializeFBOs();
	virtual bool resizeFBOs();

	virtual bool initializeFBO( unsigned int index );
	virtual bool deInitializeFBO( unsigned int index );
	virtual bool resizeFBO( unsigned int index );

	virtual void bindMultipassTextures();
	virtual void unbindMultipassTextures();

	//Regular single-pass variables
	bool isMultipass;

	std::string vertexShader;
	std::string fragmentShader;

	std::string fragmentShaderAudioSnippet;

	//Multi-pass variables
	unsigned int passes;
	unsigned int passIndex;

	std::vector< persistentFBO > multipassFBOs;
};

template< typename PluginType >
inline PluginInstance MorphFFGL::CreatePlugin( PluginInfo infos )
{
	return Plugin::CreatePlugin< PluginType >( infos, PluginType );
}

}//End namespace ffglqs