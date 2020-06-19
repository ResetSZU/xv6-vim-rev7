/*

*/

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "vim.h"
//=====================================这里是数据结构＝＝＝==========================
typedef struct row
{
    int size; //这是实际长度
    char* Tcahrs;
    uchar* colors;
}row;

typedef struct editorText
{
    char* content; // 这里我们采用一维数组的方式存储内容，不想太麻烦了
    char* colors;
    uint size;  //文本长度
    uint Textrow;//文本实际行数
    char* BeginChar;//屏幕第一行对应文本的位置，上下翻页
    row bottomMsg; // 这里就是底部状态栏了
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
int  getFileSize(char* filename);

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
   // printf(1,"%s",Text.content);
    RefreshScreen();
    //while(1)
    {
        //这里考虑放在process里面还是外面
        char ichar;
        readCharFromScreen(&ichar);
        ProcessKeyPress(ichar);
  //      RefreshScreen();
    }
    //可能要写文本，还原，重设光标，不知道放在哪里比较好
    //也许是process里面？
    exit();
}


//=======================================函数实现=================================================

void ReadfromFile(char*fileName)
{
    printf(1,"read file successfully\n");
    int fd = 0, filesize = -1,cnt=0;
    if(fileName==NULL || (fd = open(fileName, O_RDONLY))<0)
    {
        printf(1,"open error! %d \n",fd);
        Text.content = malloc(InitFileSize);
        Text.size = 0;
    }else
    {
        filesize = getFileSize(fileName);
        //这里就是初始化Text的地方,
        Text.content = malloc(filesize+InitFileSize);
        Text.size = read(fd, Text.content, filesize);
        close(fd);
        if(Text.size < 0)
            Text.size = 0;
    }
    Text.TextDirty = 0;
    printf(1,"this is filesize:%d , readsize:%d\n",filesize,Text.size);
    return Text.size;
}

int getFileSize(char* filename)
{
    struct stat stat_file;
    int filesize = -1;
    if (stat(filename, &stat_file) <0  || stat_file.type != 2)
        exit();
    filesize = (int) stat_file.size;
    return filesize;
}

void RefreshScreen()
{
    clearScreen();
    showTextToScreen(Text.BeginChar);
  //  printf(1,"refresh screen successfully\n");
}

void ProcessKeyPress(char ichar)
{
    printf(1,"deal with input successfully\n");
}

void initEditor()
{
    clearScreen();
    Text.BeginChar = Text.content;
    Text.bottomMsg.Tcahrs = malloc(ScreenMaxcol);
    Text.bottomMsg.size = ScreenMaxcol;
  //  printf(1,"init screen successfully\n");
}
void readCharFromScreen(char* ichar)
{
    printf(1,"read input from screen successfully\n");
}