#pragma once
#include <string>
#include <FFGLSDK.h>

class FFGLTemplate : public ffglqs::Plugin
{
public:
	FFGLTemplate();
	~FFGLTemplate();

	//CFFGLPlugin
	FFResult InitGL( const FFGLViewportStruct* vp ) override;
	FFResult DeInitGL() override;

	FFResult ProcessOpenGL( ProcessOpenGLStruct* inputTextures ) override;

	virtual FFResult Render( ProcessOpenGLStruct* inputTextures ) override;

private:
	ffglex::FFGLShader shader; 
	ffglex::FFGLScreenQuad quad;
};
