

/*
    打开vim后，我一般使用hw.c来测试！

    一开始是命令模式，也就是可以行首/查找字符串/翻页/删除/粘贴等等
    在命令模式下输入　‘：’　　会进入ＥＸ模式，也就是可以输入q!,wq等命令退出和保存，利用ESC进入命令模式
    在命令模式下输入　‘ｉ’   进入编辑模式，BACKSPACE不能删除，Delete才是删除键记住！，利用ESC进入命令模式
    
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

typedef struct Regexitem
{
    char* Tchars;
    int color;
}Regexitem;

typedef struct RegexArray
{
    Regexitem* regexRow;
    int rownum;
}RegexArray;

typedef struct editorText
{
    row** content; // 这里我们采用二维数组的方式存储内容，不想太麻烦了
    uint size;  //文本长度
    uint TextMallocRow; //文本申请行数
    row** BeginRow;//屏幕第一行对应文本的位置，上下翻页
    row bottomMsg; // 这里就是底部状态栏了
    uchar TextDirty;  //若没有修改文本，则不写，节约读写
    //后面看喜欢加吧
    int activatePos;
}editorText;

typedef struct editorTextPool
{
    row** content;
    uint TextPoolRow; //池子擁有行数
}editorTextPool;

static editorText Text;
static editorTextPool TextPool;
static row TmpBufferRow; 
static row MatchBufferRow;
static char editorMode ; //這個是判斷處於什麼模式：編輯模式/命令模式/ＥＸ模式
static char* fileName;
static RegexArray Regexs;

//=====================================这里就是定义函数了==================================

//读写文件函数
void ReadFromFile(char*);
void ReadFromRegex(char*);
void WriteToFile(char*);
int  getFileSize(char* );

//对于刷新屏幕做了不同的封装
void RefreshScreen();
void RefreshScreenKpos();
void showAllTextToScreen(row**);

//这里是关于主程序需要的一些
void initEditor();
void ProcessKeyPress();
void ExitProcess();
void editorEx();
void editorInsert();
void freeAllBuffer();

//光标移动的函数
int movePosRight();
int movePosLeft();
int movePosUp();
int movePosDown();

//这是正则表达式匹配的算法
void RegexMatch(row* tarRow,Regexitem* regex);
void RegexAllMatch(row* tarRow);

void readCharFromScreen(char*,int);
void setBottomMsg(const char*,int);
void Command_w();
void command_match(char*,int);

int isAlpha(char);
int itoa (int n,char* s);
void kmpPrefixFunction(char *,int ,char *);
int kmpMatch(char * ,int ,char * ,int );

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
    MatchBufferRow.Tchars = malloc(ScreenMaxcol);
    fileName = argv[1];

    //這裏好像要保存原來的screen內容，看情況吧
    ReadFromFile(fileName);
   // ReadFromRegex("Regex.txt");
    initEditor();
    //这个是让即时读取输入
    onScreenflag(1,0,0);
    RefreshScreen();
    while(1)
    {
        //这里考虑放在process里面还是外面
        ProcessKeyPress();
    //    RefreshScreen();
    }
    ExitProcess();
    //可能要写文本，还原，重设光标，不知道放在哪里比较好
    //也许是process里面？
}


//=======================================函数实现=================================================

void ReadFromFile(char*fileName)
{
    printf(1,"read file successfully\n");
    int fd = 0, filesize = -1;
    //這裏初始化池子
    TextPool.content = malloc(sizeof(row*)*FileMaxrowLen);
    TextPool.TextPoolRow = 0;
    //這裏初始化文本
    Text.content = malloc(sizeof(row*)*FileMaxrowLen);
    Text.TextMallocRow = 0;
    if(fileName==NULL || (fd = open(fileName, O_RDONLY))<0)
    {
        printf(1,"open error! %d \n",fd);
        Text.size = 0;
    }else
    {
        filesize = getFileSize(fileName);
        //这里就是初始化Text的地方,
        char* allText = malloc(filesize);
        Text.size = read(fd,allText,filesize);
        int rowcnt = 0,Textpos=0;
        while(1)
        {
            if(Textpos>=Text.size)
                break;
            newLine(Text.content+rowcnt);
            char* tchar = Text.content[rowcnt]->Tchars;
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
            rowcnt += 1;
        }
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

void WriteToFile(char* filename)
{
    int fd = 0;
    if(filename == NULL)
        filename = "noName.txt";
    fd = open(filename,O_WRONLY|O_CREATE);
    for(int i=0;i<Text.TextMallocRow;++i)
        write(fd,Text.content[i]->Tchars,Text.content[i]->size);
    close(fd);
    Text.TextDirty = 0;
}

int getFileSize(char* filename)
{
    //这里就是根据stat这个结构来得到size
    struct stat stat_file;
    int filesize = -1;
    if (stat(filename, &stat_file) <0  || stat_file.type != 2)
        filesize = -1;
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
    for(;showRow<ScreenMaxRow &&(BeginRow-Text.content)<Text.TextMallocRow && (*BeginRow)!=NULL;showRow++, BeginRow++)
    {
       // setCursorPos(ScreenMaxcol*showRow);
        showTextToScreen((*BeginRow)->Tchars,(*BeginRow)->colors,(*BeginRow)->size);
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
        free(tcontent[i]);
    }
    //釋放池子
    for(int i=0;i<TextPool.TextPoolRow;++i)
    {
        free(TextPool.content[i]->Tchars);
        free(TextPool.content[i]);
    }
    free(tcontent);
    free(TextPool.content);
}


void ProcessKeyPress()
{
    //这里是初始模式,:进入命令模式，i进入编辑模式
    char ichar ;
    updateBottomPos();
    readCharFromScreen(&ichar,1);
    unsigned char uichar = (unsigned char)(ichar);
    char*msg = "";
    int nowPos = getCursorPos();
    int trow = nowPos/ScreenMaxcol,tcol = nowPos-trow*ScreenMaxcol;
    switch (uichar)
    {
    case 'I':
        command_row(NULL);
    case 'i':
     //   printf(1,"enter insert mode\n");
        msg = "--INSERT--";
        setBottomMsg(msg,strlen(msg));
        updateBottomPos();
        editorInsert();
        break;
    case ':':
        Text.activatePos = nowPos;
        editorEx();
        break;
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
    case '0':                         //下列三个是行首/下一行首/上一行首
        command_row(NULL);
        break;
    case '-':
        command_row(movePosUp);
        break;
    case '+':
        command_row(movePosDown);
        break;
    case 'd':      //dd 两次才是删除当行，还有其他功能，不想写了
        readCharFromScreen(&ichar,1);
        if(ichar == 'd')
        {
            deleteLine(Text.BeginRow+trow);
            if(trow +Text.BeginRow-Text.content == Text.TextMallocRow)
                trow -= 1;
            setCursorPos(trow*ScreenMaxcol);
            command_row(NULL);
            RefreshScreenKpos();
        }
        break;
    case 'n':
    case 'N':
        command_match(MatchBufferRow.Tchars,MatchBufferRow.size);
        break;
    case 'w':
    case 'W':
        command_nextWord();
        break;
    case 'y':
    case 'Y':   //拷贝行
        memmove(TmpBufferRow.Tchars,Text.BeginRow[trow]->Tchars,Text.BeginRow[trow]->size);
        memmove(TmpBufferRow.colors,Text.BeginRow[trow]->colors,Text.BeginRow[trow]->size);
        TmpBufferRow.size = Text.BeginRow[trow]->size;
        break;
    case 'p':
        trow += 1;
    case 'P':
        newLine(Text.BeginRow+trow);
        memmove(Text.BeginRow[trow]->Tchars,TmpBufferRow.Tchars,TmpBufferRow.size);
        memmove(Text.BeginRow[trow]->colors,TmpBufferRow.colors,TmpBufferRow.size);
        Text.BeginRow[trow]->size = TmpBufferRow.size;
        RefreshScreenKpos();
        break;
    case '/':
        Text.activatePos = getCursorPos();
        onScreenflag(0,1,1);
        setBottomMsg("/",1);
        setCursorPos(ScreenTextMaxLen+1);
        readCharFromScreen(MatchBufferRow.Tchars,0);
        MatchBufferRow.size = strlen(MatchBufferRow.Tchars);
        MatchBufferRow.Tchars[MatchBufferRow.size-1] ='\0';
        MatchBufferRow.size -= 1;
        onScreenflag(1,0,0);
        setCursorPos(nowPos);
        command_match(MatchBufferRow.Tchars,MatchBufferRow.size);
        break;
    case CTRL_E:
        if (Text.BeginRow-Text.content != Text.TextMallocRow-1)
        {
            Text.BeginRow += 1;
            if(trow !=0)
                trow -= 1;
            else
            {
                while(tcol>=Text.BeginRow[trow]->size)
                    tcol -= 1;
            }
            setCursorPos(trow*ScreenMaxcol+tcol);
            RefreshScreenKpos();
        }
        break;
    case CTRL_Y:
        if(Text.BeginRow-Text.content != 0)
        {
            Text.BeginRow -= 1;
            if(trow != ScreenMaxRow - 1)
                trow += 1;
            else
            {
                while(tcol>=Text.BeginRow[trow]->size)
                    tcol -= 1;
            }
            setCursorPos(trow*ScreenMaxcol+tcol);
            RefreshScreenKpos();
        }
        break;
    case CTRL_F:
        Text.BeginRow += min(ScreenMaxRow,Text.TextMallocRow-1+Text.content-Text.BeginRow);
        trow = min(trow,Text.TextMallocRow+Text.content-1-Text.BeginRow);
        setCursorPos(trow*ScreenMaxcol);
        RefreshScreenKpos();
        break;
    case CTRL_B:
        Text.BeginRow = max(Text.content,Text.BeginRow-ScreenMaxRow);
        setCursorPos(trow*ScreenMaxcol);
        RefreshScreenKpos();
        break;
    default:
     //   printf(1,"%d",uichar);
        break;
    }
 //   printf(1,"deal with input successfully\n");

}

void command_match(char* tar,int tarsize)
{
  //  printf(1,"--------%s\n",tar);
    int nowPos = getCursorPos(),pos=-1;
    int trow = nowPos/ScreenMaxcol,tcol=nowPos-trow*ScreenMaxcol+1;
    for(;Text.BeginRow-Text.content+trow<Text.TextMallocRow;++trow)
    {
        row* crow = Text.BeginRow[trow];
        pos = kmpMatch(crow->Tchars+tcol,crow->size-tcol,
            tar,tarsize);
     //   printf(1,"---------%d\n",pos);
        if(pos!=-1)
            break;
        tcol = 0;
    }
    if(pos != -1)
    {
        if(trow>=ScreenMaxRow)
        {
            Text.BeginRow += (trow-ScreenMaxRow+1);
            trow = ScreenMaxRow - 1;
        }
        setCursorPos(trow*ScreenMaxcol+pos+tcol);
    }
}

void command_nextWord()
{
    int nowPos,trow,tcol;
    for(;;movePosRight())
    {
        nowPos = getCursorPos();
        trow = nowPos/ScreenMaxcol,tcol=nowPos-trow*ScreenMaxcol;
        if(!isAlpha(Text.BeginRow[trow]->Tchars[tcol]))
            break;
    }
    for(;;movePosRight())
    {
        nowPos = getCursorPos();
        trow = nowPos/ScreenMaxcol,tcol=nowPos-trow*ScreenMaxcol;
        if(isAlpha(Text.BeginRow[trow]->Tchars[tcol]))
            break;
    }
}

void command_row(int(*p)(void))
{
    if(p!=NULL)
        p();
    int nowPos = getCursorPos();
    int trow = nowPos/ScreenMaxcol,tcol;
    for(tcol=0;tcol<Text.BeginRow[trow]->size-1 &&
        Text.BeginRow[trow]->Tchars[tcol]==' ';tcol++);
    setCursorPos(trow*ScreenMaxcol+tcol);
    updateBottomPos();
}

int isAlpha(char c)
{
    return (c>='a' && c<='z') || (c>='A' && c<='Z');
}

void initEditor()
{
    Text.BeginRow = Text.content;
    Text.bottomMsg.Tchars = malloc(ScreenMaxcol);
    Text.bottomMsg.colors = malloc(ScreenMaxcol);
    memset(Text.bottomMsg.Tchars,'\0',ScreenMaxcol);
    Text.bottomMsg.size = ScreenMaxcol;
    editorMode = COMMAND_MDOE;
    clearScreen();
  //  printf(1,"init screen successfully\n");
}
void readCharFromScreen(char* ichar,int oneflag)
{
    if(oneflag)
        read(0,ichar,1);
    else
    {
        gets(ichar,ScreenMaxcol);
    }
    
   // printf(1,"%c ------- read input from screen successfully\n",*ichar);
}




void editorInsert()
{
    char ichar ;
    while(1)
    {
        readCharFromScreen(&ichar,1);
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
            setBottomMsg("",0);
            return ;
        default:
          //  printf(1,"%d",unichar);
            insertChar(ichar);
            Text.TextDirty = 1;
            RefreshScreenKpos();
          //  printf(1,"this is insert default  %d\n",unichar);
            break;
        }
    }
  //  RefreshScreen();
}
//==============================這裏是光標的移動呀START＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
//这个就是光标右移动
int movePosRight()
{
    int tpos = getCursorPos();
    int trow = tpos/ScreenMaxcol;
    int tcol = tpos - trow*ScreenMaxcol;
    int flagRefresh = 0;
   // printf(1,"this is right %d %d %d %d %d\n",Text.BeginRow-Text.content,trow,Text.TextMallocRow,tcol,Text.BeginRow[trow]->Tchars[tcol]);
    if(Text.BeginRow-Text.content+trow>=Text.TextMallocRow-1 && tcol>=Text.BeginRow[trow]->size-1)
    {
        return 0;
    }
    if(tcol==Text.BeginRow[trow]->size-1)
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
        flagRefresh = 1;
    }
    setCursorPos(ScreenMaxcol*trow + tcol);
    updateBottomPos();
    if(flagRefresh)
        RefreshScreenKpos();
    return 1;
}

int movePosLeft()
{
    int tpos = getCursorPos();
    int trow = tpos/ScreenMaxcol;
    int tcol = tpos - trow*ScreenMaxcol;
    int flagRefresh = 0;
    if(Text.BeginRow == Text.content && tpos == 0 )
        return 0;
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
        flagRefresh = 1;
    }
    setCursorPos(ScreenMaxcol*trow + tcol);
    updateBottomPos();
    if(flagRefresh)
        RefreshScreenKpos();
    return 1;
}

int movePosUp()
{
    int tpos = getCursorPos();
    int trow = tpos/ScreenMaxcol;
    int tcol = tpos - trow*ScreenMaxcol;
    int flagRefresh = 0;
    if(trow==0)
    {
        if(Text.BeginRow == Text.content)
            return 0;
        Text.BeginRow -= 1;
        flagRefresh = 1;
    }else
        trow -= 1;
    tcol = min(tcol,Text.BeginRow[trow]->size-1);
    setCursorPos(ScreenMaxcol*trow + tcol);
    updateBottomPos();
    if(flagRefresh)
        RefreshScreenKpos();
    return 1;
}

int  movePosDown()
{
    int tpos = getCursorPos();
    int trow = tpos/ScreenMaxcol;
    int tcol = tpos - trow*ScreenMaxcol;
    int flagRefresh = 0;
    if(Text.BeginRow-Text.content+trow>=Text.TextMallocRow-1)
        return 0;
    if(trow == ScreenMaxRow-1)
    {
        Text.BeginRow += 1;
        flagRefresh = 1;
    }
    else
        trow += 1;
    tcol = min(tcol,Text.BeginRow[trow]->size-1);
    setCursorPos(ScreenMaxcol*trow + tcol);
    updateBottomPos();
    if(flagRefresh)
        RefreshScreenKpos();
    return 1;
}

//==============================這裏是光標的移動呀END＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝

void insertChar(char ichar)
{
    int pos = getCursorPos();
    int trow = pos/ScreenMaxcol,tcol = pos%ScreenMaxcol;
    row* Currow = Text.BeginRow[trow];
    unsigned char uichar = (unsigned char)ichar;
   // printf(1,"this is input char%d",uichar);
    switch (uichar)
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
            moveRightNchars(TmpBufferRow.size,trow+1,tcol);
            memmove(Text.BeginRow[trow+1]->Tchars,TmpBufferRow.Tchars,TmpBufferRow.size);
        }
        setCursorPos(trow*ScreenMaxcol);
        movePosDown();
        break;
    case '\t':
        memset(TmpBufferRow.Tchars,' ',TabLength);
        TmpBufferRow.size = TabLength;
        moveRightNchars(4,trow,tcol);
        pos += 4;
        for(int i=0;i<TabLength;++i)
            movePosRight();
        break;
    case VIM_DELETE:
        moveLeftNchars(1,trow,tcol);
     //   printf(1,"yes \n");
        movePosLeft();
        break;
    default:
        TmpBufferRow.Tchars[0] = ichar;
        TmpBufferRow.size = 1;
        moveRightNchars(1,trow,tcol);
        movePosRight();
        break;
    }
    
}

void editorEx()
{
    onScreenflag(0,1,1);
    char ichar[ScreenMaxLen] ;
    char* msg ;
    setBottomMsg(":",1);
    setCursorPos(ScreenTextMaxLen+1);
    while (1)
    {
        readCharFromScreen(ichar,0);
        setBottomMsg(":",1);
        setCursorPos(ScreenTextMaxLen+1);
     //   printf(1,"------%d\n",ichar[0]);
        if(ichar[0]==VIM_ESC)
        {
            setCursorPos(Text.activatePos);
            setBottomMsg("",0);
            break;
        }
        else if(strcmp(ichar,"q\n")==0)
        {
            if(Text.TextDirty)
            {
                msg = "No write since last change (add ! to override)";
                setBottomMsg(msg,strlen(msg));
                onScreenflag(1,0,0);
                readCharFromScreen(ichar,1);
                onScreenflag(0,1,1);
                setBottomMsg("",0);
                setBottomMsg(":",1);
            }else
            {
                ExitProcess();
            }
            
        }else if(strcmp(ichar,"q!\n")==0)
        {
            ExitProcess();
        }else if(strcmp(ichar,"wq\n") == 0)
        {
            Command_w();
            ExitProcess();
        }else if(strcmp(ichar,"w\n") == 0)
        {
            Command_w();
        }else
        {
            Command_error(ichar);
            break;
          //  printf(1,"%d   %d cmp fail !!!\n",strlen(ichar),strlen("q\0"));
        }
        
        
    }
    onScreenflag(1,0,0);
}
void Command_error(char* ichar)
{
    char *msg = ":Not an editor command  :";
    memmove(TmpBufferRow.Tchars,msg,strlen(msg));
    memmove(TmpBufferRow.Tchars+strlen(msg),ichar,strlen(ichar));
    TmpBufferRow.size = strlen(msg)+strlen(ichar);
    setBottomMsg(TmpBufferRow.Tchars,TmpBufferRow.size);
    setCursorPos(Text.activatePos);
}

void Command_w()
{
    WriteToFile(fileName);
    char* msg = " are written";
    memmove(TmpBufferRow.Tchars,fileName,strlen(fileName));
    memmove(TmpBufferRow.Tchars+strlen(fileName),msg,strlen(msg));
    TmpBufferRow.size = strlen(fileName)+strlen(msg);
    setBottomMsg(TmpBufferRow.Tchars,TmpBufferRow.size);
}


void ExitProcess()
{
    //恢復備份
    setBottomMsg("",0);
    clearScreen();
    freeAllBuffer();
    onScreenflag(0,1,0);
    exit();
}

void newLine(row** irow)
{
    for(int i=Text.TextMallocRow;Text.content+i!=irow-1;--i)
    {
        Text.content[i+1] = Text.content[i];
    }
    if(TextPool.TextPoolRow>0)
    {
        TextPool.TextPoolRow -= 1;
        (*irow) = TextPool.content[TextPool.TextPoolRow];
        TextPool.content[TextPool.TextPoolRow] = NULL;
    }else
    {
        (*irow) = malloc(sizeof(row));
        (*irow)->Tchars = malloc(ScreenMaxcol+1);
        (*irow)->colors = malloc(ScreenMaxcol+1);
    }
    (*irow)->size = 0;
    memset((*irow)->Tchars,'\0',ScreenMaxcol+1);
    memset((*irow)->colors,'\0',ScreenMaxcol+1);
    Text.TextMallocRow += 1;
}

void deleteLine(row** irow)
{
    if(TextPool.TextPoolRow<FileMaxrowLen)
    {
        TextPool.content[TextPool.TextPoolRow] = *irow;
        TextPool.TextPoolRow += 1;
    }else
    {
        free((*irow)->Tchars);
        free((*irow)->colors);
        free(*irow);
    }
    for(int i=irow-Text.content;i<Text.TextMallocRow;++i)
        Text.content[i] = Text.content[i+1];
    Text.content[Text.TextMallocRow-1] = NULL;
    Text.TextMallocRow -= 1;
}
//BeginRow爲base,在行列处向右移動n个字符
void moveRightNchars(int n,int trow,int tcol)
{
    //memmove(void *dst, const void *src, uint n)
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

//BeginRow爲base,在行列处向左移動n个字符
void moveLeftNchars(int n,int trow,int tcol)
{
    row** brow = Text.BeginRow;
    char tmprow[ScreenMaxcol] ;
    int tmpRowSize = 0;
    while(1)
    {
        if(trow+brow-Text.content==Text.TextMallocRow)
            return ;
        if(brow[trow]->Tchars[tcol]=='\n'  )
        {
            if(brow[trow]->size<=1)
            {
                deleteLine(brow+trow);
                break;
            }
         //   printf(1,"%d\n",brow[trow]->size);
            TmpBufferRow.size = ScreenMaxcol-tcol;
            if(trow+brow-Text.content==Text.TextMallocRow-1)
            {
                brow[trow]->Tchars[tcol] = '\0';
                brow[trow]->size -= 1;
                break;
            }else
            {
                memmove(brow[trow]->Tchars+tcol,brow[trow+1]->Tchars,
                    min(brow[trow+1]->size,TmpBufferRow.size));
                brow[trow]->size += min(brow[trow+1]->size,TmpBufferRow.size)-1;
                n = min(brow[trow+1]->size,TmpBufferRow.size);
                trow += 1;
                tcol = 0;
            }
        }
        if(brow[trow]->Tchars[brow[trow]->size-1]=='\n')
        {
            if(n>=brow[trow]->size-tcol)
            {
         //       printf(1,"---------------\n");
                memset(brow[trow]->Tchars[tcol],'\0',ScreenMaxcol-tcol);
                if(tcol == 0)
                    deleteLine(brow+trow);
                else
                    brow[trow]->size = tcol;
            }
            else
            {
                memmove(brow[trow]->Tchars+tcol,brow[trow]->Tchars+tcol+n,
                    brow[trow]->size-tcol-n);
                memset(brow[trow]->Tchars+brow[trow]->size-n,'\0',ScreenMaxcol-brow[trow]->size+n);
                brow[trow]->size -= n;
            }
         //   printf(1,"--------------------end\n");
            break;
        }else
        {
            memmove(brow[trow]->Tchars+tcol,brow[trow]->Tchars+tcol+n,
                    brow[trow]->size-tcol-n);
            memset(brow[trow]->Tchars+brow[trow]->size-n,'\0',ScreenMaxcol-brow[trow]->size+n);
            if(trow+brow-Text.content<Text.TextMallocRow-1)
            {
                memmove(brow[trow]->Tchars+brow[trow]->size-n,brow[trow+1]->Tchars
                    ,min(ScreenMaxcol-brow[trow]->size+n,brow[trow+1]->size));
                brow[trow]->size += min(ScreenMaxcol-brow[trow]->size+n,brow[trow+1]->size)-n;
            }else
                brow[trow]->size -= n;
            trow += 1;
            tcol = 0;
            
        }
        
    }
}

void setBottomMsg(const char* stateMsg,int tlen)
{
    if(tlen != 0)
    {
        memset(Text.bottomMsg.Tchars,'\0',50);
        memmove(Text.bottomMsg.Tchars,stateMsg,tlen);
    }else
    {
        memset(Text.bottomMsg.Tchars,'\0',ScreenMaxcol);
    }
    
    int nowPos = getCursorPos();
    setCursorPos(ScreenMaxRow*ScreenMaxcol);
    showTextToScreen(Text.bottomMsg.Tchars,Text.bottomMsg.colors,Text.bottomMsg.size-1);
    setCursorPos(nowPos);
}

//這個是更新底部右側坐標的是
void updateBottomPos()
{
    int nowPos = getCursorPos();
    if(Text.activatePos == nowPos)
        return ;
    int offset = 50;
    //這裏是坐標
    memset(Text.bottomMsg.Tchars+offset,'\0',ScreenMaxcol-offset);
    offset += itoa(nowPos/ScreenMaxcol,Text.bottomMsg.Tchars+offset);
    Text.bottomMsg.Tchars[offset++] = ',';
    offset += itoa(nowPos%ScreenMaxcol,Text.bottomMsg.Tchars+offset);
    offset = 75;
    //這是裏百分比記號
    if(Text.content == Text.BeginRow)
        memmove(Text.bottomMsg.Tchars+offset,"Top",3);
    else if(Text.BeginRow-Text.content+ScreenMaxRow>=Text.TextMallocRow)
        memmove(Text.bottomMsg.Tchars+offset,"Bot",3);
    else
    {
        int rate = 100*(Text.BeginRow-Text.content)/(Text.TextMallocRow-ScreenMaxRow);
        if(rate <10)
            offset+= 1;
        offset += itoa(rate,Text.bottomMsg.Tchars+offset);
        Text.bottomMsg.Tchars[offset++] = '%';
    }
    setCursorPos(ScreenMaxRow*ScreenMaxcol);
    showTextToScreen(Text.bottomMsg.Tchars,Text.bottomMsg.colors,Text.bottomMsg.size-1);
    setCursorPos(nowPos);
    Text.activatePos = nowPos;
}

int itoa (int n,char* s)
{
  int i,j,sign;
  if((sign=n)<0)//记录符号
  n=-n;//使n成为正数
  i=0;
  do{
      s[i++]=n%10+'0';//取下一个数字
  }
  while ((n/=10)>0);//删除该数字
  if(sign<0)
  s[i++]='-';
  for(int j=0;j<i/2;++j)
  {
    char tmp = s[j];
    s[j] = s[i-1-j];
    s[i-1-j] = tmp;
  }
  return i;
}


//KMP匹配算法

int kmpMatch(char * s,int sLength,char * p,int pLength)
{
    if(sLength<=0 || pLength <= 0)
        return -1;
    char* prefix = TmpBufferRow.Tchars;
    kmpPrefixFunction(p,pLength,prefix);
    int pPoint=0;
    p[pLength] = '\0';
  //  printf(1,"%s  %d\n",s,sLength);
    for(int i=0; i<sLength;i++)
    {
    //    printf(1,"%c   %c\n",s[i],p[pPoint]);
 
        while(pPoint!=0&&(s[i]!=p[pPoint]))
        {
            pPoint = prefix[pPoint-1];
        }
        if(s[i]==p[pPoint])
        {
            pPoint++;
            if(pPoint == pLength)
            {
            //    printf(1,"find :%d \n",i-pPoint+1);
                return (i-pPoint+1);
                //pPoint = 0;//上一个在s匹配的字符串,不能成为下一个匹配字符串的一部分
                pPoint=prefix[pPoint-1];//上一个在s匹配的字符串,也能成为下一个匹配字符串的一部分
            }
        }
    }
    //printf(1,"Not find\n");
    return -1;
}
 
void kmpPrefixFunction(char *p,int length,char *prefix)
{
    prefix[0]=0;
    int k = 0;//前缀的长度
    for(int i=1; i<length; i++)
    {
        while(k>0&&p[k]!=p[i])
        {
            k=prefix[k-1];
        }
        if(p[k]==p[i])//说明p[0...k-1]共k个都匹配了
        {
            k=k+1;
        }
        prefix[i]=k;
    }
}

void ReadFromRegex(char* fileName)
{
    int rownum = 0;
    int fd = -1;
    if(fileName == NULL ||(fd = open(fileName,O_RDONLY)) < 0)
    {
        Regexs.regexRow = NULL;
        Regexs.rownum = 0;
        return ;
    }
    Regexs.regexRow = malloc(sizeof(Regexitem)*FileMaxrowLen);
    int filesize = getFileSize(fileName);
    char* allText = malloc(filesize);
    filesize = read(fd,allText,filesize);
    int rowcnt = 0,Textpos =0 ,FirstSpace=-1;
    while(1)
    {
        if(Textpos>=filesize)
            break;
        char* tchar = TmpBufferRow.Tchars;
        int i = 0;
        for(;i<ScreenMaxcol && Textpos<Text.size;i++,Textpos++)
        {
            tchar[i] = allText[Textpos];
            if(FirstSpace == -1 && tchar[i]==' ')
                FirstSpace = i+1;
            if(tchar[i] == '\n')
            {
                i += 1;
                Textpos += 1;
                break;
            }
        }
        Regexs.regexRow[rowcnt].Tchars = malloc(i-FirstSpace);
        memmove(Regexs.regexRow[rowcnt].Tchars,TmpBufferRow.Tchars+FirstSpace,i-FirstSpace);
        Regexs.regexRow[rowcnt].Tchars[i-1-FirstSpace] = '\0';
        Regexs.regexRow[rowcnt].color = atoi(TmpBufferRow.Tchars);
        printf(1,"%s  %d\n",Regexs.regexRow[rowcnt].Tchars,Regexs.regexRow[rowcnt].color);
        rowcnt += 1;
    }
    Regexs.rownum = rowcnt;
    free(allText);
    close(fd);
    return ;
}

void RegexAllMatch(row* tarRow)
{
    for(int i=0;i<Regexs.rownum;++i)
        RegexMatch(tarRow,&(Regexs.regexRow[i]));
    return ;
}

void RegexMatch(row* tarRow,Regexitem* regex)
{
    int matchLength,matchidx;
    //这里避免BUG
    tarRow->Tchars[tarRow->size] = '\0';
    matchidx = re_match(regex->Tchars,tarRow->Tchars,&matchLength);
    memset(tarRow->colors+matchidx,regex->color,matchLength-2);
}