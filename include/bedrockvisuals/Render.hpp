#pragma once
namespace bedrockvisuals::render {
bool install();
void uninstall();
bool ready();
void setFill(void* fn);
void setFinish(void* fn);
void onUiFinish(void* ctx);
}
