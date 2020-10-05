// by renqihong

// compile:
// gcc -g -rdynamic -o main main.c  -std=c99 -lpthread

// execute: ./main
//
// Config search paths:config.txt
// add a path per line in config.txt like this:
//
// How to use:
// 1、Search path or file name include with 123 and 456 and 789 in the same time:
// 123(Space Key)456(Space Key)789
//
// 2、Search path or file name include 123 and search the text contain 456:
// 123(TAB)456
//
// 3、Clear history input keys:
// key1(SPACE or TAB) key2(ENTER)
//
// 4、Clear update index:
// `(near by keys of number 1 and ESC)


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <termios.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <linux/inotify.h>

pthread_t th_A,th_B,th_C;

time_t now;
struct tm *tm_now;

#define PRINT_ONLY_TIME(...)time(&now);\
                            tm_now = localtime(&now);\
                           printf("%02d-%02d-%02d %02d:%02d:%02d ",tm_now->tm_year+1900, tm_now->tm_mon, tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec); \
                            printf(__VA_ARGS__); \
                            printf("\n");

#define PRINT_WITH_TIME(...)time(&now);\
                            tm_now = localtime(&now);\
                            printf("%02d-%02d-%02d %02d:%02d:%02d FILE:%s LINE:%d FUNC:%s ",tm_now->tm_year+1900, tm_now->tm_mon, tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec, __FILE__, __LINE__, __FUNCTION__); \
                            printf(__VA_ARGS__); \
                            printf("\n");

#define PRINT_WITH_TIME_NOLN(...)time(&now);\
                            tm_now = localtime(&now);\
                            printf("%02d-%02d-%02d %02d:%02d:%02d FILE:%s LINE:%d FUNC:%s ",tm_now->tm_year+1900, tm_now->tm_mon, tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec, __FILE__, __LINE__, __FUNCTION__); \
                            printf(__VA_ARGS__);

#define PRINTLN(...) printf(__VA_ARGS__); \
                            printf("\n");

static int flag = 1;

#define NONE                 "\e[0m"
#define BLACK                "\e[0;30m"
#define L_BLACK              "\e[1;30m"
#define RED                  "\e[0;31m"
#define L_RED                "\e[1;31m"
#define GREEN                "\e[0;32m"
#define L_GREEN              "\e[1;32m"
#define BROWN                "\e[0;33m"
#define YELLOW               "\e[1;33m"
#define BLUE                 "\e[0;34m"
#define L_BLUE               "\e[1;34m"
#define PURPLE               "\e[0;35m"
#define L_PURPLE             "\e[1;35m"
#define CYAN                 "\e[0;36m"
#define L_CYAN               "\e[1;36m"
#define GRAY                 "\e[0;37m"
#define WHITE                "\e[1;37m"

#define BOLD                 "\e[1m"
#define UNDERLINE            "\e[4m"
#define BLINK                "\e[5m"
#define REVERSE              "\e[7m"
#define HIDE                 "\e[8m"
#define CLEAR                "\e[2J"
#define CLRLINE              "\r\e[K" //or "\e[1K\r"

bool isSearchRunning = false;
bool isSearchString = false;
#define INIT_FILE_SIZE 4000000
#define PATH_LENGTH 500
#define PATH_NUM 10
#define INVALID_INDEX -1

int IncludePathCount = 0; //handleIncludePath index
int ExcludePathCount = 0; //handleExcludePath index
char handleIncludePath[PATH_NUM][PATH_LENGTH];
char handleExcludePath[PATH_NUM][PATH_LENGTH];
char pathTemp[PATH_LENGTH] = {0};
#define KEY_LENGTH 100
#define STRING_LENGTH 20
#define PRINT_LOG(...) printf("%s %s %d ", __FILE__, __FUNCTION__, __LINE__); \
                            printf(__VA_ARGS__); \
                            printf("\n");

#define EVENT_SIZE ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

long fileCount = 0;
long allCount = 0;
char key[KEY_LENGTH];
char keys[KEY_LENGTH];
char string[STRING_LENGTH];
int keyCount = 0;
int stringKeyCount = 0;
int keysCount = 0;

// press Enter key
bool isResearch = false;

// press Space key
bool isNewKey = false;

typedef struct
{
    char *path;
    int nextIndex;
    int d_type;
}ITEM, *PITEM;

PITEM pAllFiles[INIT_FILE_SIZE];
long long pathLengthAll = 0;

int getch(void)
{
    struct termios oldattr, newattr;
    int ch;
    tcgetattr( STDIN_FILENO, &oldattr );
    newattr = oldattr;
    newattr.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newattr );
    ch = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldattr );
    return ch;
}

int addFile(const char *path, int d_type)
{
    size_t size = strlen(path);
    if(size <= 0)
    {
        PRINT_WITH_TIME("size error: %s", path);
        return 1;
    }
    pAllFiles[allCount] = (PITEM)malloc(sizeof(ITEM));
    //PRINT_WITH_TIME("path: %s, size: %d, allCount: %d, sizeof ITEM: %d", path, strlen(path), allCount, sizeof(ITEM));

    if (pAllFiles[allCount] == NULL)
    {
        PRINT_WITH_TIME("malloc error");
    }

    pAllFiles[allCount]->path = (char *)malloc(size + 1);
    if (pAllFiles[allCount] == NULL)
    {
        PRINT_WITH_TIME("malloc error");
    }
    memset(pAllFiles[allCount]->path, 0, size + 1);
    strcpy(pAllFiles[allCount]->path, path);
    pAllFiles[allCount]->nextIndex = INVALID_INDEX;
    pAllFiles[allCount]->d_type= d_type;
    //PRINT_WITH_TIME("i:%ld %s", allCount, pAllFiles[allCount]->path);
    size += sizeof(ITEM);
    pathLengthAll += size;
    allCount++;

    return 0;
}

int readAllFile(char *basePath)
{
    //PRINT_WITH_TIME("%s", basePath);

    DIR *dir;
    struct dirent *ptr;

    if ((dir = opendir(basePath)) == NULL)
    {
        PRINT_WITH_TIME("Open dir error...: %s, %ld", basePath, strlen(basePath));
        return 1;
        //exit(1);
    }

    while((ptr = readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0)    ///current dir OR parrent dir
        {
            continue;
        }
        else if(ptr->d_type == 8 || ptr->d_type == 10 || ptr->d_type == 4)    ///file
        {
            char base[PATH_LENGTH];
            //printf("ptr->d_name length: %d\n", strlen(ptr->d_name));
            memset(base,'\0', PATH_LENGTH);
            memcpy(base, basePath, strlen(basePath));

            if(0 != strcmp("/", basePath))
            {
                strcat(base, "/");
            }
            strcat(base, ptr->d_name);
            //PRINT_WITH_TIME("%d %d basePath: %s, length: %d", fileCount, folderCount, base, strlen(base));
            addFile(base, ptr->d_type);

            if(ptr->d_type == 4)
            {
                int i = 0;
                bool andReadList = true;
                //PRINT_WITH_TIME("ExcludePathCount:%d", ExcludePathCount)
                for(i = 0; i < ExcludePathCount; i++)
                {
                    if(NULL != strstr(base, handleExcludePath[i]))
                    {
                        //readAllFile(base);
                        andReadList = false;
                        break;
                    }
                }

                if(andReadList)
                {
                    readAllFile(base);
                    //PRINT_WITH_TIME("base:%s, exclude path:%s, location:%d", base, handleExcludePath[i], strstr(base, handleExcludePath[i]))
                }

            }
            fileCount++;
            if(fileCount % 100000 == 0)
            {
                PRINTLN("Current count: %d", (int)fileCount)
            }
        }
    }
    closedir(dir);
    return 1;
}

long newfileCount;
int count;

long long getTime()
{
    struct timeval tim;
    gettimeofday (&tim , NULL);
    return (long long)tim.tv_sec*1000000+tim.tv_usec;
}

bool isSearchOver = false;
long firstMatchedIndex = INVALID_INDEX;
long finalMatchedIndex = INVALID_INDEX;

long i;
long firstIndex = 0;

long matchLineCount = 1;

int printWithColor(char *pcString)
{
    //PRINT_WITH_TIME("%s", pcString)
    printf("" L_RED "%s" NONE "", pcString);
}

void normal_match(char * s,char * p)
{
    int sLength = strlen(s);
    int pLength = strlen(p);
    int k;
    int lastMatchedNum = 0;
    int i;
    for(i=0;i<sLength;i++)
    {
        for(k=0;k<pLength;k++)
        {
            if(s[i+k]!=p[k])
            {
                break;
            }
        }
        if(k==pLength)
        {
            if(lastMatchedNum == 0)
            {
                printf("%.*s", i, s);
            }
            else
            {
                printf("%.*s", i - lastMatchedNum - pLength, s + lastMatchedNum + pLength);
            }
            //printf("%d \n",i);
            lastMatchedNum = i;
            printWithColor(p);
        }
    }
    printf("%.*s", i - lastMatchedNum - pLength, s + lastMatchedNum + pLength);
}

int searchStringInFile(char *pPath, char *pKey)
{
    if(NULL == pPath)
    {
        PRINT_WITH_TIME("pPath is NULL")
        return 0;
    }

    if(NULL == pKey)
    {
        PRINT_WITH_TIME("pKey is NULL")
        return 0;
    }

    //PRINT_WITH_TIME("path:%s key:%s", pPath, pKey);

    int lineNum = 1;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    int keyLength = (int)strlen(pKey);
    //PRINT_WITH_TIME("keyLength:%d", keyLength)

    FILE *fp=fopen(pPath,"r");
    if(fp == NULL)
    {
        PRINT_WITH_TIME("open file error")
        exit(0);
    }
    while ((read = getline(&line, &len, fp)) != -1)
    {
        memset(pathTemp, 0, PATH_LENGTH);
        if(strstr(line, pKey) != NULL)
        {
            printf("%ld %s %d ", matchLineCount, pPath, lineNum);
            normal_match(line, pKey);
            matchLineCount++;
            //putchar('\n');
        }

        lineNum++;
    }
    if (line)
    {
        free(line);
    }

    fclose(fp);

    return 0;
}

void  *handleSearch()
{
    //PRINT_WITH_TIME("");
    i = firstIndex;
    //PRINT_LOG("");
    long long start = getTime();

    if(isSearchRunning == false)
    {
        return 0;
    }
    /*PRINT_WITH_TIME("key:%s, isSearchOver:%d, isSearchRunning:%d, isNewKey: %d, firtsMatchedIndex:%d",
                    key,
                    isSearchOver,
                    isSearchRunning,
                    isNewKey,
                    firstMatchedIndex)*/

    //PRINT_WITH_TIME("")

    if(isResearch == true)
    {
        newfileCount = fileCount;
        isResearch = false;
    }

    //PRINT_WITH_TIME("count: %d, keyCount: %d, isNewKey: %d", count, keyCount, isNewKey);

    if(isNewKey == true && keyCount == 1)
    {
        //PRINT_WITH_TIME("new key: %d, keyCount: %d", count, keyCount);
        //memcpy(AllListTemp, AllListTemp2, count * sizeof(ITEM));
        newfileCount = count;
    }

    if(isSearchRunning != false)
    {
        count = 1;
    }

    finalMatchedIndex = INVALID_INDEX;
    firstMatchedIndex = INVALID_INDEX;
    //PRINT_WITH_TIME("i:%d", i)
    long temp = 0;
    //long finalMatchedIndexTemp = finalMatchedIndex;

    //??????????10000ms sleep????
    while(1)
    {

        temp = pAllFiles[i]->nextIndex;

        if(isSearchRunning == false)
        {
            break;
        }

        if(isSearchRunning == false || isSearchOver == true)
        {
            //PRINT_LOG("");
            break;
        }

        char *p = strstr(pAllFiles[i]->path, key);

        // handle search string in text
        if(isSearchString)
        {
            //PRINT_WITH_TIME("path:%s key:%s", pAllFiles[i]->path, string)
            searchStringInFile(pAllFiles[i]->path, string);

        }
            //printf("%d %s %s\n", i, AllList[i].cs_basePath, AllList[i].cs_name);
        else if(p != NULL)
        {
            //puts(p);
            //PRINT_WITH_TIME("%s",p)
            if(firstMatchedIndex == INVALID_INDEX)
            {
                firstMatchedIndex = i;
                //PRINT_WITH_TIME("firtsMatchedIndex:%ld", firstMatchedIndex);
                /*PRINT_WITH_TIME("count:%d key:%s i: %ld, path:%s",
                       count,
                       key,
                       i,
                       pAllFiles[i]->path);
            */
                printf("%d" /*L_RED "%s" NONE*/ " %ld/%ld ",
                       count,
                        /*key,*/
                       i,
                       allCount);

                normal_match(pAllFiles[i]->path, key);
                putchar('\n');
            }


            if(finalMatchedIndex != INVALID_INDEX)
            {
                pAllFiles[finalMatchedIndex]->nextIndex = i;
                /*PRINT_WITH_TIME("count:%d key:%s %ld/%ld pAllFiles[%d]->nextIndex:%ld path:%s",
                       count,
                       key,
                       i,
                       newfileCount,
                       finalMatchedIndex,
                       pAllFiles[finalMatchedIndex]->nextIndex,
                       pAllFiles[i]->path);
                */

                printf("%d" /*L_RED "%s" NONE*/ " %ld/%ld ",
                       count,
                        /*key,*/
                       i,
                       allCount);

                normal_match(pAllFiles[i]->path, key);
                putchar('\n');

            }

            pAllFiles[i]->nextIndex = INVALID_INDEX;
            finalMatchedIndex = i;
            count++;
        }


        if(isNewKey == true)
        {
            i = temp;
            //PRINT_WITH_TIME("pAllFiles[%ld]->nextIndex:%ld", i, pAllFiles[i]->nextIndex);
            if(i == INVALID_INDEX)
            {
                break;
            }
        }
        else
        {
            i++;
            //PRINT_WITH_TIME("i: %d", i)

        }
        //PRINT_WITH_TIME("i:%ld, newfileCount:%ld", i, newfileCount);
        if(i == newfileCount)
        {
            break;
        }

        if(i % 10000 == 0)
        {
            //PRINT_LOG("Sleep ");
            usleep(1000);
        }

        //PRINT_LOG("key: %s, i: %d, isSearchRunning: %d", key, i, isSearchRunning);

    } //for

    if(i == newfileCount)
    {
        isSearchOver = true;
    }
    if(isSearchRunning != false)
    {
        //PRINTLN("%d " L_RED "%s" NONE " %ld/%ld %s",
        printf( "Search \"" L_RED  "%s" NONE  "\" result:\n", keys);
        printf("" L_RED "%ld " NONE  "items found, cost" L_RED " %lld " NONE "ms\n", isSearchString ? matchLineCount : --count, (getTime() - start) / 1000);
    }
    isSearchRunning = false;

    /*PRINT_LOG("");*/
    pthread_exit("slave_thread!!");
}


bool isSearch = false;

/* reads from keypress, doesn't echo */
void handleRead(void)
{
    if(isSearchRunning)
    {
        pthread_cancel(th_B);
        usleep(1000);
    }

    //PRINT_WITH_TIME("");
    memset(key, 0, KEY_LENGTH * sizeof(char));
    memset(keys, 0, KEY_LENGTH * sizeof(char));
    memset(string, 0, STRING_LENGTH* sizeof(char));

    while(1)
    {
        char x=getch();
        //PRINT_WITH_TIME("%c", x);

        isSearch = false;

        if (96 == x) //键盘1左边的符号:“~”英文符号叫“ Tilde” ,(意思是 颚化符号,鼻音化符号,代字号)。中文俗称“波浪号”。
        {
            PRINT_WITH_TIME("Refresh index");
            system("Found");
        }
        else if(x == '\b') //Backspace key: Last char of key deleted
        {
            //PRINT_WITH_TIME("Backspace");
            printf("Last char of key deleted!\n");
            isSearchRunning = false;
            isSearchOver = false;
            usleep(1000);
            printf("\n");
            keyCount--;
            key[keyCount] = 0;
            keysCount--;
            keys[keysCount] = 0;
            isSearch = true;
            //search(key, isNewKey, isResearch);

            isSearchRunning = true;
            if(keyCount > 0 || keysCount >0)
            {
                pthread_create(&th_B, NULL, handleSearch, NULL);
            }
            //createSlaveThread();
            usleep(1000);
            //printResult();
        }
        else if(x == '\n') //Enter key: Clear history inputted keys
        {
            //PRINT_WITH_TIME("Enter");
            firstMatchedIndex = INVALID_INDEX;
            //printf("Enter\n");
            printf("All history input keys cleared!\n");
            firstIndex = 0;
            isResearch = true;
            isSearchOver = false;
            isSearchString = false;
            matchLineCount = 0;
            keyCount = 0;
            isNewKey = false;
            memset(key, 0, KEY_LENGTH * sizeof(char));
            memset(string, 0, STRING_LENGTH * sizeof(char));
            memset(keys, 0, KEY_LENGTH * sizeof(char));
            stringKeyCount = 0;
            keysCount = 0;
        }
        else if(x == 0x20) //Space key: Handle to search a new key
        {
            //PRINT_WITH_TIME("Space");

            printf("Please input a new key to search base on just now!\n");

            isNewKey = true;
            if(firstMatchedIndex != INVALID_INDEX)
            {
                firstIndex = firstMatchedIndex;
            }
            //printf("Space\n");
            keys[keysCount++] = ' ';

            isSearchString = false;
            stringKeyCount = 0;
            memset(string, 0, STRING_LENGTH * sizeof(char));

            keyCount = 0;
            memset(key, 0, KEY_LENGTH * sizeof(char));
        }
        else if(x == 9) //Tab key: Handle to search string
        {
            isSearchString = true;
            isNewKey = true;
            if(firstMatchedIndex != INVALID_INDEX)
            {
                firstIndex = firstMatchedIndex;
            }
            PRINT_WITH_TIME("tab: %d", x);
            memset(string, 0, STRING_LENGTH* sizeof(char));
            stringKeyCount = 0;
            keys[keysCount++] = ' ';
            keys[keysCount++] = 's';
            keys[keysCount++] = ':';
            printf("Please input a string search in text base on just now!\n");

        }
        else if(x >= '0' && x <= '9'     //Other key: add a new char at the end of current key
                || x>= 'a' && x <= 'z'
                || x>= 'A' && x <= 'Z'
                || x == '-'
                || x == '.'
                || x == '_'
                || x == '/'
                || x == '\\')
        {
            //PRINT_WITH_TIME("Please input a new char added on current key!");
            //PRINTLN("Please input a new char added on current key!");
            matchLineCount = 0;
            isSearchOver = false;
            if (isSearchRunning) {
                pthread_cancel(th_B);
                isSearchRunning = false;
                usleep(1000);
            }
            if (isNewKey != true) {
                isSearchRunning = false;
                //createSlaveThread();
                usleep(1000);
            }

            if (isSearchString) {
                string[stringKeyCount++] = x;
                keys[keysCount++] = x;
            } else {
                key[keyCount++] = x;
                keys[keysCount++] = x;
            }
            //putchar(x);
            putchar('\n');
            //if(keyCount == 4)
            //{
            isSearchRunning = true;
            //search(key, isNewKey, isResearch);
            pthread_create(&th_B, NULL, handleSearch, NULL);
            usleep(1000);
            //printResult();
            isResearch = false;
            //}
        }
        else
        {
            printf("Illagel input!!: %d\n", x);
        }

        //usleep(100000);
    }

    pthread_exit("master_thread!!");
}

int k = 0;
void thread_C()
{
}

int main (void)
{
    isSearchRunning = false;

    PRINTLN("Let's start!");
    char basePath[PATH_LENGTH];

    ///get the current absoulte path
    //memset(basePath,'\0',sizeof(basePath));
    getcwd(basePath, 999);

    ///get the file list
    //memset(basePath,'\0',sizeof(basePath));

    memset(handleIncludePath, '\0', PATH_NUM * PATH_LENGTH);
    memset(handleExcludePath, '\0', PATH_NUM * PATH_LENGTH);

    FILE * fp_config_file = fopen("config.txt", "r");
    if(NULL == fp_config_file)
    {
        PRINT_WITH_TIME("open config.txt error");
        exit(1);
    }

    memset(basePath, '\0', sizeof(basePath));
    while(NULL != (fgets(basePath, PATH_LENGTH, fp_config_file)))
    {
        //PRINT_WITH_TIME("get path in config.txt:%s", basePath);
        if(43 == basePath[0]) // '+'
        {
            memcpy(handleIncludePath[IncludePathCount], basePath + 1, strlen(basePath) - 2); //+ and \0
            PRINT_WITH_TIME("Include path:%s", handleIncludePath[IncludePathCount]);
            IncludePathCount++;
        }
        else if( 45 == basePath[0]) // '-'
        {
            memcpy(handleExcludePath[ExcludePathCount], basePath + 1, strlen(basePath) - 2); //+ and \0
            PRINT_WITH_TIME("Exclude path:%s", handleExcludePath[ExcludePathCount]);
            ExcludePathCount++;
        }
        else
        {
            PRINT_WITH_TIME("Input error");
        }
        memset(basePath, '\0', sizeof(basePath));
    }
    fclose(fp_config_file);

    int i = 0;
    while(handleIncludePath[i][0] != '\0')
    {
        memset(basePath, '\0', sizeof(basePath));
        memcpy(basePath, handleIncludePath[i], strlen(handleIncludePath[i]));
        //basePath[5] = '\0';
        printf("Add files in path: %s\n", basePath);
        puts(basePath);
        readAllFile(basePath);
        i++;
    }

    //添加排除目录
    PRINTLN("Read files end");

    PRINTLN("Files count:%ld, pathLengthAll:%lld, perPathLength:%lld", fileCount, pathLengthAll, pathLengthAll/fileCount);

    newfileCount = fileCount;

    void *thread_result;
    //PRINT_WITH_TIME("This is the main process");

    int res = pthread_create(&th_A, NULL, (void*)&handleRead, NULL);
    if(res != 0)
    {
        //PRINT_WITH_TIME("");
        exit(EXIT_FAILURE);
    }


    res = pthread_create(&th_B, NULL, (void*)&handleSearch, NULL);
    if(res != 0)
    {
        //PRINT_WITH_TIME("");
        exit(EXIT_FAILURE);
    }

    pthread_detach(th_A);

    while(1)
    {
        usleep(1000);
    }
    //PRINT_WITH_TIME("");

}
