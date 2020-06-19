/*

*/

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

//=====================================宏定义＝＝＝＝==============================

//屏幕的长宽
#define ScreenMaxRow 25 
#define ScreenMaxcol 80 

//=====================================这里是数据结构＝＝＝==========================
typedef struct row
{
    int size; //这是实际长度，考虑到换行
    char* content;
    uchar* colors;
}row;

typedef struct editorText
{
    row* content; // 这里我们采用二维数组的方式存储内容，不想太麻烦了
    uint size;  //文本长度
    uint Textrow;//文本实际行数
    uint currentScreenRow;//屏幕第一行对应文本第几行，上下翻页
    row *bottomMsg; // 这里就是底部状态栏了
    uchar screendirty; //节省速度
    uchar TextDirty;  //若没有修改文本，则不写，节约读写
    //后面看喜欢加吧
}editorText;

static editorText Text;
//=====================================这里就是定义函数了==================================

void ReadfromFile(char*fileName);
void RefreshScreen();
void ProcessKeyPress(char ichar);
void initEditor();
void readCharFromScreen(char* ichar);

//=====================================主程序===============================================

int main(int argc, char **argv)
{
    if( argc != 2)
    {
        //這裏就vim xxx or vim
        printf(1,"error");
        exit();
    }    
    char* fileName = argv[1];

    //這裏好像要保存原來的screen內容，看情況吧
    ReadfromFile(fileName);
    initEditor();
    RefreshScreen();
   // setCursorPos(0,0);
    //while(1)
    {
        //这里考虑放在process里面还是外面
        char ichar;
        readCharFromScreen(&ichar);
        ProcessKeyPress(ichar);
        RefreshScreen();
    }
    //可能要写文本，还原，重设光标，不知道放在哪里比较好
    //也许是process里面？
    exit();
}


//=======================================函数实现=================================================

void ReadfromFile(char*fileName)
{
    printf(1,"read file successfully\n");
}

void RefreshScreen()
{
    printf(1,"refresh screen successfully\n");
}

void ProcessKeyPress(char ichar)
{
    printf(1,"deal with input successfully\n");
}

void initEditor()
{
    printf(1,"init screen successfully\n");
}
void readCharFromScreen(char* ichar)
{
    printf(1,"read input from screen successfully\n");
}