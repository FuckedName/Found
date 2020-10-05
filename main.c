// by renqihong

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

pthread_t th_A,th_B;

time_t now;
struct tm *tm_now;

#define PRINT_WITH_TIME(...)time(&now);\
                            tm_now = localtime(&now);\
                            printf("%02d-%02d-%02d %02d:%02d:%02d FILE:%s LINE:%d FUNC:%s ",tm_now->tm_year+1900, tm_now->tm_mon, tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec, __FILE__, __LINE__, __FUNCTION__); \
                            printf(__VA_ARGS__); \
                            printf("\n");

#define PRINTLN(...) printf(__VA_ARGS__); \
                            printf("\n");

#define NONE                 "\e[0m"
#define L_RED                "\e[1;31m"

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

long fileCount = 0;
long allCount = 0;
char key[KEY_LENGTH];
char keys[KEY_LENGTH];
char string[STRING_LENGTH];
int keyCount = 0;
int stringKeyCount = 0;
int keysCount = 0;
bool isResearch = false;
bool isNewKey = false;

typedef struct
{
    char *path;
    long nextIndex;
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
    size += sizeof(ITEM);
    pathLengthAll += size;
    allCount++;

    return 0;
}

FILE *InitConfigFile(const char *puc_FileName)
{
    FILE *pFile = fopen(puc_FileName, "r");

    if (pFile == NULL)
    {
        pFile = fopen(puc_FileName, "w");
        int ret = fputs("+/\n", pFile);
        if (EOF == ret)
        {
            PRINT_WITH_TIME("Write into config.txt error")
            fclose(pFile);
            exit(1);
        }
        fclose(pFile);
    }

    return pFile;
}

int readAllFile(char *basePath)
{
    DIR *dir;
    struct dirent *ptr;

    if ((dir = opendir(basePath)) == NULL)
    {
        PRINT_WITH_TIME("Open dir error...: %s, %ld", basePath, strlen(basePath));
        return 1;
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
            memset(base,'\0', PATH_LENGTH);
            memcpy(base, basePath, strlen(basePath));

            if(0 != strcmp("/", basePath))
            {
                strcat(base, "/");
            }
            strcat(base, ptr->d_name);
            addFile(base, ptr->d_type);

            if(ptr->d_type == 4)
            {
                int i = 0;
                bool andReadList = true;
                for(i = 0; i < ExcludePathCount; i++)
                {
                    if(NULL != strstr(base, handleExcludePath[i]))
                    {
                        andReadList = false;
                        break;
                    }
                }

                if(andReadList)
                {
                    readAllFile(base);
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
    return (long long)tim.tv_sec * 1000000 + tim.tv_usec;
}

bool isSearchOver = false;
long firstMatchedIndex = INVALID_INDEX;
long finalMatchedIndex = INVALID_INDEX;

long i;
long firstIndex = 0;

long matchLineCount = 1;

int printWithColor(char *pcString)
{
    printf("" L_RED "%s" NONE "", pcString);
}

void normal_match(char * s,char * p)
{
    int sLength = strlen(s);
    int pLength = strlen(p);
    int k;
    int lastMatchedNum = 0;
    int i;
    for(i = 0; i < sLength; i++)
    {
        for(k = 0; k < pLength; k++)
        {
            if(s[i + k] != p[k])
            {
                break;
            }
        }
        if(k == pLength)
        {
            if(lastMatchedNum == 0)
            {
                printf("%.*s", i, s);
            }
            else
            {
                printf("%.*s", i - lastMatchedNum - pLength, s + lastMatchedNum + pLength);
            }
            lastMatchedNum = i;
            printWithColor(p);
        }
    }
    printf("%.*s", i - lastMatchedNum - pLength, s + lastMatchedNum + pLength);
}

int searchStringInFile(char *pPath, char *pKey)
{
    if(NULL == pPath || NULL == pKey)
    {
        PRINT_WITH_TIME("pPath or pKey is NULL")
        return 0;
    }

    int lineNum = 1;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    FILE *fp=fopen(pPath, "r");
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
    i = firstIndex;
    long long start = getTime();

    if(isSearchRunning == false)
    {
        return 0;
    }

    if(isResearch == true)
    {
        newfileCount = fileCount;
        isResearch = false;
    }

    if(isNewKey == true && keyCount == 1)
    {
        newfileCount = count;
    }

    if(isSearchRunning != false)
    {
        count = 1;
    }

    finalMatchedIndex = INVALID_INDEX;
    firstMatchedIndex = INVALID_INDEX;
    long temp = 0;

    while(1)
    {
        temp = pAllFiles[i]->nextIndex;

        if(isSearchRunning == false)
        {
            break;
        }

        if(isSearchRunning == false || isSearchOver == true)
        {
            break;
        }

        char *p = strstr(pAllFiles[i]->path, key);

        if(isSearchString)
        {
            searchStringInFile(pAllFiles[i]->path, string);

        }
        else if(p != NULL)
        {
            if(firstMatchedIndex == INVALID_INDEX)
            {
                firstMatchedIndex = i;
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
            if(i == INVALID_INDEX)
            {
                break;
            }
        }
        else
        {
            i++;

        }
        if(i == newfileCount)
        {
            break;
        }

        if(i % 10000 == 0)
        {
            usleep(1000);
        }

    } //for

    if(i == newfileCount)
    {
        isSearchOver = true;
    }
    if(isSearchRunning != false)
    {
        printf( "Search \"" L_RED  "%s" NONE  "\" result:\n", keys);
        printf("" L_RED "%ld " NONE  "items found, cost" L_RED " %lld " NONE "ms\n", isSearchString ? matchLineCount : --count, (getTime() - start) / 1000);
    }
    isSearchRunning = false;

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

    memset(key, 0, KEY_LENGTH * sizeof(char));
    memset(keys, 0, KEY_LENGTH * sizeof(char));
    memset(string, 0, STRING_LENGTH* sizeof(char));

    while(1)
    {
        char x=getch();

        isSearch = false;

        if (96 == x) //键盘1左边的符号:“~”英文符号叫“ Tilde” ,(意思是 颚化符号,鼻音化符号,代字号)。中文俗称“波浪号”。
        {
            PRINT_WITH_TIME("Refresh index");
            system("Found");
        }
        else if(x == '\b') //Backspace key: Last char of key deleted
        {
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

            isSearchRunning = true;
            if(keyCount > 0 || keysCount >0)
            {
                pthread_create(&th_B, NULL, handleSearch, NULL);
            }
            usleep(1000);
        }
        else if(x == '\n') //Enter key: Clear history inputted keys
        {
            firstMatchedIndex = INVALID_INDEX;
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

            printf("Please input a new key to search base on just now!\n");

            isNewKey = true;
            if(firstMatchedIndex != INVALID_INDEX)
            {
                firstIndex = firstMatchedIndex;
            }
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
            matchLineCount = 0;
            isSearchOver = false;
            if (isSearchRunning)
            {
                pthread_cancel(th_B);
                isSearchRunning = false;
                usleep(1000);
            }
            if (isNewKey != true)
            {
                isSearchRunning = false;
                usleep(1000);
            }

            if (isSearchString)
            {
                string[stringKeyCount++] = x;
                keys[keysCount++] = x;
            }
            else
            {
                key[keyCount++] = x;
                keys[keysCount++] = x;
            }
            putchar('\n');
            isSearchRunning = true;
            pthread_create(&th_B, NULL, handleSearch, NULL);
            usleep(1000);
            isResearch = false;
        }
        else
        {
            printf("Illagel input!!: %d\n", x);
        }
    }
}

int main (void)
{
    isSearchRunning = false;

    PRINTLN("Let's start!");
    char basePath[PATH_LENGTH];

    memset(handleIncludePath, '\0', PATH_NUM * PATH_LENGTH);
    memset(handleExcludePath, '\0', PATH_NUM * PATH_LENGTH);

    FILE *fp_config_file = InitConfigFile("config.txt");

    memset(basePath, '\0', sizeof(basePath));
    while(NULL != (fgets(basePath, PATH_LENGTH, fp_config_file)))
    {
        PRINT_WITH_TIME("get path in config.txt:%s", basePath);
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
        printf("Add files in path: %s\n", basePath);
        puts(basePath);
        readAllFile(basePath);
        i++;
    }

    PRINTLN("Read files finished.");

    PRINTLN("Files count:%ld, pathLengthAll:%lld, perPathLength:%lld", fileCount, pathLengthAll, pathLengthAll/fileCount);

    newfileCount = fileCount;

    int res = pthread_create(&th_A, NULL, (void*)&handleRead, NULL);
    if(res != 0)
    {
        exit(EXIT_FAILURE);
    }

    res = pthread_create(&th_B, NULL, (void*)&handleSearch, NULL);
    if(res != 0)
    {
        exit(EXIT_FAILURE);
    }

    pthread_detach(th_A);

    while(1)
    {
        usleep(1000);
    }
}
