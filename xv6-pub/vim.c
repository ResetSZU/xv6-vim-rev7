/*

*/

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "vim.h"
#include "re.h"

//=====================================这里是数据结构＝＝＝==========================
typedef struct row
{
    int size; //这是实际长度
    char* Tchars;
    uchar* colors;
}row;

typedef struct editorText
{
    row** content; // 这里我们采用二维数组的方式存储内容，不想太麻烦了
    uint size;  //文本长度
    uint Textrow;//文本实际行数
    uint TextMallocRow; //文本申请行数
    row** BeginRow;//屏幕第一行对应文本的位置，上下翻页
    char* AcitveChar; //光标动作发生处
    row bottomMsg; // 这里就是底部状态栏了
    uchar TextDirty;  //若没有修改文本，则不写，节约读写
    //后面看喜欢加吧
}editorText;

static editorText Text;
static row TmpBufferRow; 
//=====================================这里就是定义函数了==================================

void ReadfromFile(char*fileName);
void RefreshScreen();
void ProcessKeyPress();
void initEditor();
void readCharFromScreen(char* ichar);
void movePosRight();
void movePosLeft();
void movePosUp();
void movePosDown();
void showAllTextToScreen(row**);
void RefreshScreenKpos();
void freeAllBuffer();
int  getFileSize(char* filename);
inline int max(int lhs,int rhs) { return lhs>rhs?lhs:rhs;};
inline int min(int lhs,int rhs) { return lhs<rhs?lhs:rhs;};

//=====================================主程序===============================================

int main(int argc, char **argv)
{
    if( argc != 2)
    {
        //這裏就vim xxx or vim
        printf(1,"error");
        exit();
    }    
    TmpBufferRow.Tchars = malloc(ScreenMaxcol);
    char* fileName = argv[1];

    //這裏好像要保存原來的screen內容，看情況吧
    ReadfromFile(fileName);
    initEditor();
    //这个是让即时读取输入
    onScreenflag(1,0);
    RefreshScreen();
    while(1)
    {
        //这里考虑放在process里面还是外面
        ProcessKeyPress();
    //    RefreshScreen();
    }
    freeAllBuffer();
    onScreenflag(0,1);
    //可能要写文本，还原，重设光标，不知道放在哪里比较好
    //也许是process里面？
    exit();
}


//=======================================函数实现=================================================

void ReadfromFile(char*fileName)
{
    printf(1,"read file successfully\n");
    int fd = 0, filesize = -1;
    if(fileName==NULL || (fd = open(fileName, O_RDONLY))<0)
    {
        printf(1,"open error! %d \n",fd);
        Text.content = malloc(sizeof(row*)*FileMaxrowLen);
        Text.content[0] = NULL;
        Text.TextMallocRow = 0;
        Text.Textrow = 0;
        Text.size = 0;
    }else
    {
        filesize = getFileSize(fileName);
        //这里就是初始化Text的地方,
        Text.content = malloc(sizeof(row*)*FileMaxrowLen);
        char* allText = malloc(filesize);
        Text.size = read(fd,allText,filesize);
        int rowcnt = 0,Textpos=0;
        while(1)
        {
            if(Textpos>=Text.size)
                break;
            Text.content[rowcnt] = malloc(sizeof(row));
            char* tchar = malloc(ScreenMaxcol);
            int i = 0;
            for(;i<ScreenMaxcol && Textpos<Text.size;i++,Textpos++)
            {
                tchar[i] = allText[Textpos];
                if(tchar[i] == '\n')
                {
                    i += 1;
                    Textpos += 1;
                    break;
                }
            }
            Text.content[rowcnt]->size = i;
            Text.content[rowcnt]->Tchars = tchar;
            rowcnt += 1;
        }
        Text.TextMallocRow = rowcnt ;
        Text.content[rowcnt] = NULL;
        Text.BeginRow = Text.content;
        free(allText);
        close(fd);
        if(Text.size < 0)
            Text.size = 0;
    }
    Text.TextDirty = 0;
    printf(1,"this is filesize:%d , readsize:%d  row is %d\n",filesize,Text.size,Text.TextMallocRow);
    return ;
}

int getFileSize(char* filename)
{
    //这里就是根据stat这个结构来得到size
    struct stat stat_file;
    int filesize = -1;
    if (stat(filename, &stat_file) <0  || stat_file.type != 2)
        exit();
    filesize = (int) stat_file.size;
    return filesize;
}

void RefreshScreenKpos()
{
    int pos = getCursorPos();
    RefreshScreen();
    setCursorPos(pos);
}

void RefreshScreen()
{
    clearScreen();
    showAllTextToScreen(Text.BeginRow);
   // printf(1,"refresh screen successfully\n");
}

void showAllTextToScreen(row** BeginRow)
{
    int showRow = 0;
    for(;showRow<ScreenMaxRow && (*BeginRow)!=NULL;showRow++, BeginRow++)
    {
       // setCursorPos(ScreenMaxcol*showRow);
        showTextToScreen((*BeginRow)->Tchars,(*BeginRow)->size);
    }
}

//釋放內存呀
void freeAllBuffer()
{
    if(TmpBufferRow.Tchars!=NULL)
        free(TmpBufferRow.Tchars);
    row** tcontent = Text.content;
    for(int i=0;i<Text.TextMallocRow && tcontent[i]!=NULL;++i)
    {
        free(tcontent[i]->Tchars);
        tcontent[i]->Tchars = NULL;
        tcontent[i]->size = 0;
    }
}


void ProcessKeyPress()
{
    //这里是初始模式,:进入命令模式，i进入编辑模式
    char ichar ;
    readCharFromScreen(&ichar);
    switch (ichar)
    {
    case 'i':
     //   printf(1,"enter insert mode\n");
        editorInsert();
        break;
    case ':':
      //  editorCommond();
        break;
    
    default:
        break;
    }
 //   printf(1,"deal with input successfully\n");

}

void initEditor()
{
    Text.BeginRow = Text.content;
    Text.bottomMsg.Tchars = malloc(ScreenMaxcol);
    Text.bottomMsg.size = ScreenMaxcol;
    clearScreen();
  //  printf(1,"init screen successfully\n");
}
void readCharFromScreen(char* ichar)
{
    read(0,ichar,1);
   // printf(1,"%c ------- read input from screen successfully\n",*ichar);
}




void editorInsert()
{
    char ichar ;
    while(1)
    {
        readCharFromScreen(&ichar);
        unsigned char unichar = (unsigned char)(ichar);
        int pos = getCursorPos();
        switch (unichar)
        {
        case VIM_UP:
            movePosUp();
            break;
        case VIM_DOWN:
            movePosDown();
            break;
        case VIM_LEFT:
            movePosLeft();
            break;
        case VIM_RIGHT:
            movePosRight();
            break;
        case VIM_ESC:
            return;
        default:
            insertChar(ichar);
            RefreshScreenKpos();
          //  printf(1,"this is insert default  %d\n",unichar);
            break;
        }
    }
  //  RefreshScreen();
}
//这个就是光标右移动
void movePosRight()
{
    int tpos = getCursorPos();
    int trow = tpos/ScreenMaxcol;
    int tcol = tpos - trow*ScreenMaxcol;
   // printf(1,"this is right %d %d %d %d %d\n",Text.BeginRow-Text.content,trow,Text.TextMallocRow,tcol,Text.BeginRow[trow]->Tchars[tcol]);
    if(Text.BeginRow-Text.content+trow>=Text.TextMallocRow-1 && tcol>=Text.BeginRow[trow]->size-1)
    {
        return ;
    }
    if(tcol==ScreenMaxcol-1 || Text.BeginRow[trow]->Tchars[tcol]=='\n' || tcol==Text.BeginRow[trow]->size)
    {
        trow += 1;
        tcol = 0;
    }else
    {
        tcol += 1;
    }
    
    if(trow>=ScreenMaxRow)
    {
        Text.BeginRow += 1;
        trow  = ScreenMaxRow-1;
    }
    setCursorPos(ScreenMaxcol*trow + tcol);
}

void movePosLeft()
{
    int tpos = getCursorPos();
    int trow = tpos/ScreenMaxcol;
    int tcol = tpos - trow*ScreenMaxcol;
    if(Text.BeginRow == Text.content && tpos == 0 )
        return ;
    tcol -= 1;
    if(tcol<0)
    {
        trow -= 1;
        tcol = Text.BeginRow[trow]->size - 1;
    }
    if(trow<0)
    {
        Text.BeginRow -= 1;
        trow = 0;
    }
    setCursorPos(ScreenMaxcol*trow + tcol);
}

void movePosUp()
{
    int tpos = getCursorPos();
    int trow = tpos/ScreenMaxcol;
    int tcol = tpos - trow*ScreenMaxcol;
    int flagRefresh = 0;
    if(trow==0)
    {
        if(Text.BeginRow == Text.content)
            return ;
        Text.BeginRow -= 1;
        flagRefresh = 1;
    }else
        trow -= 1;
    tcol = min(tcol,Text.BeginRow[trow]->size-1);
    setCursorPos(ScreenMaxcol*trow + tcol);
    if(flagRefresh)
        RefreshScreenKpos();
}

void movePosDown()
{
    int tpos = getCursorPos();
    int trow = tpos/ScreenMaxcol;
    int tcol = tpos - trow*ScreenMaxcol;
    int flagRefresh = 0;
    if(Text.BeginRow-Text.content+trow>=Text.TextMallocRow-1)
        return ;
    if(trow == ScreenMaxRow-1)
    {
        Text.BeginRow += 1;
        flagRefresh = 1;
    }
    else
        trow += 1;
    tcol = min(tcol,Text.BeginRow[trow]->size-1);
    setCursorPos(ScreenMaxcol*trow + tcol);
    if(flagRefresh)
        RefreshScreenKpos();
}

void insertChar(char ichar)
{
    int pos = getCursorPos();
    int trow = pos/ScreenMaxcol,tcol = pos%ScreenMaxcol;
    row* Currow = Text.BeginRow[trow];
    printf(1,"this is row %d  col %d\n",trow,tcol);
    printf(1,"%s",Currow->Tchars);
    printf(1,"%s",Text.content[0]->Tchars);
    printf(1,"%s",Text.BeginRow[0]->Tchars);
    switch (ichar)
    {
    case '\n':  //这里需要判断是不是段尾，所以很麻烦
        memmove(TmpBufferRow.Tchars,Currow->Tchars+tcol,Currow->size-tcol);
        TmpBufferRow.size = Currow->size-tcol;
        if(Currow->Tchars[Currow->size-1]=='\n')
        {
            Text.BeginRow[trow]->Tchars[tcol] = '\n';
            memset(Currow->Tchars+tcol+1,'\0',ScreenMaxcol-tcol-1);
            Text.BeginRow[trow]->size = tcol+1;
            newLine(Text.BeginRow+trow+1);
            memmove(Text.BeginRow[trow+1]->Tchars,TmpBufferRow.Tchars,TmpBufferRow.size);
            Text.BeginRow[trow+1]->size = TmpBufferRow.size;
        }else
        {
            Text.BeginRow[trow]->Tchars[tcol] = '\n';
            memset(Currow->Tchars+tcol+1,'\0',ScreenMaxcol-tcol-1);
            moveNchars(TmpBufferRow.size,trow+1,tcol);
            memmove(Text.BeginRow[trow+1]->Tchars,TmpBufferRow.Tchars,TmpBufferRow.size);
        }
        setCursorPos(trow*ScreenMaxcol);
        movePosDown();
        break;
    case '\t':
        memset(TmpBufferRow.Tchars,' ',TabLength);
        TmpBufferRow.size = TabLength;
        moveNchars(4,trow,tcol);
        setCursorPos(pos+4);
        break;
    default:
        TmpBufferRow.Tchars[0] = ichar;
        TmpBufferRow.size = 1;
        moveNchars(1,trow,tcol);
        setCursorPos(pos+1);
        break;
    }
    
}

void newLine(row** irow)
{
    for(int i=Text.TextMallocRow;Text.content+i!=irow-1;--i)
    {
        Text.content[i+1] = Text.content[i];
    }
    (*irow) = malloc(sizeof(row));
    (*irow)->size = 0;
    (*irow)->Tchars = malloc(ScreenMaxcol);
    memset((*irow)->Tchars,'\0',ScreenMaxcol);
    Text.TextMallocRow += 1;
}

//BeginRow爲base,在行列处移動n个字符
void moveNchars(int n,int trow,int tcol)
{
    //memmove(void *dst, const void *src, uint n)
  //  printf(1,"movechar %d\n",n);
    row** brow = Text.BeginRow;
    char tmprow[ScreenMaxcol] ;
    int tmpRowSize = 0;
    while(1)
    {
        if(brow[trow] == NULL)
        {
            newLine(brow);
            if(trow>=ScreenMaxRow)
            {
                Text.BeginRow = (brow += trow-ScreenMaxRow+1);
                trow = ScreenMaxRow-1;
            }
        }
        char* tchar = brow[trow]->Tchars;
        int tsize = brow[trow]->size;
        if(tsize+n<=ScreenMaxcol) //這裏是段落尾部了可以結束了
        {
           // printf(1,"this is tsize %d\n",tsize);
            memmove(tchar+tcol+n,tchar+tcol,tsize-tcol);
            memmove(tchar+tcol,TmpBufferRow.Tchars,TmpBufferRow.size);
            brow[trow]->size += n;
            break;
            
        }else
        {
            tmpRowSize = tsize+TmpBufferRow.size-ScreenMaxcol;
            memmove(tmprow,tchar+tsize-tmpRowSize,tmpRowSize); //保存该行尾部的内容
            memmove(tchar+tcol+tmpRowSize,tchar+tcol,tsize-tcol-tmpRowSize);
            memmove(tchar+tcol,TmpBufferRow.Tchars,TmpBufferRow.size);
            memmove(TmpBufferRow.Tchars,tmprow,tmpRowSize);
            TmpBufferRow.size = tmpRowSize;
            if(TmpBufferRow.Tchars[tmpRowSize-1]=='\n')
            {
                newLine(brow+trow+1);
                memmove(brow[trow+1]->Tchars,TmpBufferRow.Tchars,TmpBufferRow.size);
                brow[trow+1]->size = TmpBufferRow.size;
                if(trow>=ScreenMaxRow)
                {
                    Text.BeginRow = (brow += trow-ScreenMaxRow+1);
                    trow = ScreenMaxRow-1;
                }
                break; ;
            }
            tcol = 0;trow += 1;
        }
        
    }
}