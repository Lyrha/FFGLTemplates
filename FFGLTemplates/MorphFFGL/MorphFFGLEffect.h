#pragma once
#include "MorphFFGL.h"

namespace ffglqs
{

class MorphFFGLEffect : public MorphFFGL
{
public:
	//Call this constructor for single-pass plugins
	MorphFFGLEffect(
		std::string vertShader,
		std::string fragShader);

	//Call this constructor for multi-pass plugins
	MorphFFGLEffect(
		std::string vertShader,
		std::string fragShader,
		std::vector< std::string > bufferNames );

	virtual FFResult Render( ProcessOpenGLStruct* inputTextures ) override;

	template< typename PluginType >
	static PluginInstance CreatePlugin( PluginInfo infos );

protected:
	virtual void bindMultipassTextures() override;
	virtual void unbindMultipassTextures() override;
};

template< typename PluginType >
inline PluginInstance MorphFFGLEffect::CreatePlugin( PluginInfo infos )
{
	return MorphFFGL::CreatePlugin< PluginType >( infos, FF_EFFECT );
}

}//End namespace ffglqs