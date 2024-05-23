#include "MediaContext.h"

using namespace std;



#undef main
int main()
{


    try
    {
        MediaContext* mediaCtx = new MediaContext;


        SDL_Event event;                    //定义事件
        while(1)
        {
            SDL_WaitEvent(&event);
            if( event.type == SDL_QUIT)                         //程序退出
            {
                mediaCtx->setThreadQuit(true);
                break;
            }
            else if(event.type == SDL_KEYDOWN)                  //空格键暂停
            {
                if(event.key.keysym.sym == SDLK_SPACE)
                {
                    mediaCtx->setThreadPause(!mediaCtx->getThreadQuit());
                }
            }
        }

    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }



    return 0;
}
