#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <pthread.h>
#include <semaphore.h>
  
// Defenitions
#define RANGE 1
#define ALLDONE 2
#define MAXTHREAD 10
#define MAXGRID 40
#define GO 3
#define GENDONE 4
#define STOP 5

int numOfThreads = 0;
int totalGens = 0;
int currentGen = 0;

char* filename;

int totalRows = 0;
int totalCols = 0;
int printEachGeneration = 0;
int gameEarlyExit = 0;


int evenGeneration[MAXGRID][MAXGRID];
int oddGeneration[MAXGRID][MAXGRID];




// global variables
struct msg {
    int iSender; /* sender of the message (0 .. number-of-threads) */
    int type; /* its type */
    int value1; /* first value */
    int value2; /* second value */
};

struct mailbox {
    int threadIndex;
    pthread_t readerThreadId;
    struct msg message;
    sem_t readSem;
    sem_t writeSem;
};

struct mailbox mailboxes[MAXTHREAD+1];


// functions **************************************************************
void SendMsg(int iTo, struct msg *pMsg);
void RecvMsg(int iFrom, struct msg *pMsg);
void *parentThread(void* index);
void *childThread(void *index);

int readInMatrixFile(char *filename, int *rows, int *cols, int matrix[MAXGRID][MAXGRID]);
void printMatrix(int rows, int cols, int m[MAXGRID][MAXGRID]);

void sendRowRangeMessage(struct msg *message, int index);
void initializeMatrix(int m[MAXGRID][MAXGRID]);
int isEven(int generation);
void sendGoMessageToChildren(struct msg *message, int index);
void waitForResponses(struct msg *message, int sender, int msgType);

int getVerdict(int cellValue, int nCount);
void PlayOne(int startRow, int endRow, int rows, int cols, int Old[MAXGRID][MAXGRID], int New[MAXGRID][MAXGRID]);
int equal(int rows, int cols, int n[MAXGRID][MAXGRID], int m[MAXGRID][MAXGRID]);
int allZeros(int rows, int cols, int m[MAXGRID][MAXGRID]);
void sendStopMessageToChildren(struct msg *message, int index);

// end of functions **************************************************************


int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("not enough arguments\n");
    }

    numOfThreads = atoi(argv[1]);
    totalGens = atoi(argv[3]);
    filename = argv[2];



    if (argc == 5 && !strcmp(argv[4], "y"))
    {
        // printf("Printing each generation\n");
        printEachGeneration = 1;
    }
    

    if (numOfThreads < 1)
    {
        printf("Usage: number of threads must be in range 1..%d\n", MAXTHREAD);
        return -1;
    }
    if (numOfThreads > MAXTHREAD)
    {
        numOfThreads = MAXTHREAD;
    }
    if (totalGens < 1)
    {
        totalGens = 1;
    }

    if (readInMatrixFile(filename, &totalRows, &totalCols, evenGeneration) == 0)
    {
        printf("Error reading in the matrix from file\n");
        return -1;
    }

    
    // init mailboxes and semaphores *************************************************************************************
    for (int i = 0; i < MAXTHREAD+1; i++) {
        mailboxes[i].threadIndex = i;
        sem_init(&mailboxes[i].readSem, 0, 0); // reciving sems start at 0
        sem_init(&mailboxes[i].writeSem, 0, 1); // sending sems start at 1
    }
    

    //init threads *******************************************************************************************************
    pthread_create(&mailboxes[0].readerThreadId, NULL, parentThread, (void *)&mailboxes[0].threadIndex);
    for (int i = 1; i <= numOfThreads; i++) {
        pthread_create(&mailboxes[i].readerThreadId, NULL, childThread, (void *)&mailboxes[i].threadIndex); // store thread id in mailbox struct, send index of id
    }
    
   
    // destroy everything ***************************************************************************************
    for (int i = 0; i < numOfThreads; i++) {
        pthread_join(mailboxes[i].readerThreadId, NULL);
    }
    for (int i = 0; i < MAXTHREAD+1; i++) {
        sem_destroy(&mailboxes[i].readSem);
        sem_destroy(&mailboxes[i].writeSem);
    }

    return 0;
}

// reads in a matrix 
int readInMatrixFile(char *filename, int *rows, int *cols, int matrix[MAXGRID][MAXGRID]) {
  FILE *f;
  int c, k;
  int maxStrLength = 100;
  char s[maxStrLength];

  f = fopen(filename, "r");

  // first figure out how many rows and columns there are in the matrix
  int n_rows = 0;
  int n_cols = 0;

  while (fgets(s, maxStrLength, f)) // read in one line/row at a time
  {
    int len = strlen(s); // get string length
    if (len == 0)
    {
      continue;
    }
    c = 0;
    for (k = 0; k < len; k++) // k for looping over line of chars
    {
      if (s[k] == '1' || s[k] == '0')
      {
        matrix[n_rows][c] = s[k] == '1' ? 1 : 0;
        c++; // current column index - only increment when seeing 0 or a 1

        // when processing the first row, we're also trying to figure out how many columns there are
        // so only count on the first row
        if (n_rows == 0)
        {
          n_cols++;
        }
      }
    }
    n_rows++;
  }

  *rows = n_rows;
  *cols = n_cols;
  fclose(f);
  return 1;

}

// printsa given matrix
void printMatrix(int rows, int cols, int m[MAXGRID][MAXGRID])
{
  int i, j;
  for (i = 0; i < rows; i++)
  {
    for (j = 0; j < cols; j++)
    {
      printf("%d ", m[i][j]);
    }
    printf("\n");
  }
  printf("\n");
}


  
// The function to be executed by child threads
void *childThread(void *index)
{
    int thisThreadIndex = *((int *)index);
    struct msg message;
    int playing = 1;
    int startRow, endRow;

   

    while (playing && currentGen < totalGens +1 && !gameEarlyExit) {
         RecvMsg(thisThreadIndex, &message);

        if (message.type == RANGE) {
           startRow = message.value1;
           endRow = message.value2; 
        }
        else if (message.type == GO) {

            if (currentGen >= totalGens + 1 || gameEarlyExit) {
                playing = 0;
            } else {
                PlayOne(startRow, endRow, totalRows, totalCols,
                    /*OLD*/ isEven(currentGen) ? oddGeneration : evenGeneration,
                    /*NEW*/ isEven(currentGen) ? evenGeneration : oddGeneration);   
                message.iSender = thisThreadIndex;
                message.type = GENDONE;
                SendMsg(0, &message);
            }
        }else if (message.type == STOP) {
            gameEarlyExit = 1;
            playing = 0;
        }     
    }

    // sleep(1);
    message.iSender = thisThreadIndex;
    message.type = ALLDONE;
    SendMsg(0, &message);

    pthread_exit(NULL);
    
    
}

void *parentThread(void* index) {

    int thisThreadIndex = *((int *)index);
    struct msg message;
    int threads = numOfThreads;
    int i;

    
    sendRowRangeMessage(&message, thisThreadIndex);

    
    if (printEachGeneration == 1) {
      printf("Generation [0]\n");
       printMatrix(totalRows, totalCols, evenGeneration);
    }
   


    for (i = 1; i <= totalGens; i++) {

        currentGen = i;
        if (isEven(currentGen)) {
            initializeMatrix(evenGeneration);
        } else {
            initializeMatrix(oddGeneration);
        }

        sendGoMessageToChildren(&message, thisThreadIndex);
        waitForResponses(&message, thisThreadIndex, GENDONE);
        
        if (equal(totalRows, totalCols, oddGeneration, evenGeneration) || allZeros(totalRows, totalCols, isEven(currentGen) ? evenGeneration : oddGeneration))
        {
            gameEarlyExit = 1;
            sendStopMessageToChildren(&message, thisThreadIndex);
            if (allZeros(totalRows, totalCols, isEven(currentGen) ? evenGeneration : oddGeneration)) {
                printf("Game ended, all died\n");
            }else if (equal(totalRows, totalCols, oddGeneration, evenGeneration)) {
                printf("Game ended, pattern unchanged\n");
            }
            break;
        }
        if (printEachGeneration == 1) {
          printf("Generation [%d]\n", i);
          printMatrix(totalRows, totalCols, isEven(currentGen) ? evenGeneration : oddGeneration);
        }
        
        
    }

    if (i >= totalGens) {
        printf("Reached max generations\n");
    }

    // send another GO message for threads to notice that the game is done
    currentGen++;
    sendGoMessageToChildren(&message, thisThreadIndex);

    
    waitForResponses(&message, thisThreadIndex, ALLDONE);
    pthread_exit(NULL);
   
    
}

void sendStopMessageToChildren(struct msg *message, int index) {
    message->iSender = index;
    message->type = STOP;
    message->value1 = 0;
    message->value2 = 0;
    for (int i = 1; i <= numOfThreads; i++) {
        SendMsg(i, message);
    }
}

void sendGoMessageToChildren(struct msg *message, int index) {
    message->iSender = index;
    message->type = GO;
    message->value1 = 0;
    message->value2 = 0;
    for (int i = 1; i <= numOfThreads; i++) {
        SendMsg(i, message);
    }
}

void sendRowRangeMessage(struct msg *message, int index) {
    int rowsPerThread = (int)( totalRows/ numOfThreads);
    int remainder = totalRows%numOfThreads;
    int i;
    

    message->iSender = index;
    message->type = RANGE;
    message->value1 = 1;
    message->value2 = rowsPerThread;

    for (i = 1; i <= numOfThreads; i++) {
        SendMsg(i, message);
        message->value1+=rowsPerThread;
        if (i == numOfThreads - 1 && remainder > 0) {
            message->value2 += rowsPerThread + remainder;
            
        } else {
            message->value2+=rowsPerThread;
        }
        
        
    }
}

void initializeMatrix(int m[MAXGRID][MAXGRID])
{
  int i, j;
  for (i = 0; i < MAXGRID; i++)
  {
    for (j = 0; j < MAXGRID; j++)
    {
      m[i][j] = 0;
    }
  }
}

int isEven(int generation)
{
  return generation % 2 == 0;
}

void waitForResponses(struct msg *message, int sender, int msgType)
{
  int numThreads = numOfThreads;

  while (numThreads > 0)
  {
    RecvMsg(sender, message);
    if (message->type == msgType)
    {
      numThreads--;
    }
  }

}
  

void SendMsg(int iTo, struct msg *pMsg) {
    sem_wait(&mailboxes[iTo].writeSem);

    mailboxes[iTo].message.iSender = pMsg->iSender;
    mailboxes[iTo].message.type = pMsg->type;
    mailboxes[iTo].message.value1 = pMsg->value1;
    mailboxes[iTo].message.value2 = pMsg->value2;

    sem_post(&mailboxes[iTo].readSem);


}

void RecvMsg(int iFrom, struct msg *pMsg) {

   // this thread will wait on the mailbox-read semaphore until it gets woken up by the sender
  sem_wait(&mailboxes[iFrom].readSem);

  pMsg->iSender = mailboxes[iFrom].message.iSender;
  pMsg->type = mailboxes[iFrom].message.type;
  pMsg->value1 = mailboxes[iFrom].message.value1;
  pMsg->value2 = mailboxes[iFrom].message.value2;

  // notify the next thread in line waitning to write a message to mailbox that this thread is done
  // processing the message
  sem_post(&mailboxes[iFrom].writeSem);
}

void PlayOne(int startRow, int endRow, int rows, int cols, int Old[MAXGRID][MAXGRID], int New[MAXGRID][MAXGRID])
{
  /* loop through array New, setting each array element to zero or
    one depending up its neighbors in Old.*/

  int i, j, nCount;

  // printf("PlayOne: rows[%d..%d]\n", startRow, endRow);
  // count neighbors for each cell[i][j]
  for (i = startRow; i <= endRow; i++)
  {
    for (j = 0; j < cols; j++)
    {
      nCount = 0;

      // check col to the left
      if (i > 0)
      {
        if (j > 0)
        {
          nCount += Old[i - 1][j - 1];
        }
        nCount += Old[i - 1][j];
        if (j < cols - 1)
        {
          nCount += Old[i - 1][j + 1];
        }
      }

      // check left and right
      if (j > 0)
      {
        nCount += Old[i][j - 1];
      }
      if (j < cols - 1)
      {
        nCount += Old[i][j + 1];
      }

      // check right col
      if (i < rows - 1)
      {
        if (j > 0)
        {
          nCount += Old[i + 1][j - 1];
        }
        nCount += Old[i + 1][j];
        if (j < cols - 1)
        {
          nCount += Old[i + 1][j + 1];
        }
      }

      New[i][j] = getVerdict(Old[i][j], nCount);
    }
  }
} // PlayOne

/**
 * Get verdict for the current cell - death, life or birth.
 */
int getVerdict(int cellValue, int nCount)
{

  int verdict = 0;
  switch (nCount)
  {
  case 0:
  case 1:
  case 4:
  case 5:
  case 6:
  case 7:
  case 8:
  {
    verdict = 0; // death
    break;
  }
  case 2:
  {
    // if alive, then keep it alive
    if (cellValue == 1)
    {
      verdict = 1;
    }
    else
    {
      verdict = 0;
    }
    break;
  }
  case 3:
  {
    // if alive, then keep alive
    // or
    // if cell is not ocupied (0) and there are 3 neighbors, than birth
    verdict = 1;
    break;
  }
  default:
  {
    printf("Invalid cell value %d\n", nCount);
  }
  } // end of swtich

  // if (nCount > 0) {
  // printf("Verdict:  #count %d   %d => %d\n", nCount, cellValue, verdict);
  // }
  return verdict;
}


int equal(int rows, int cols, int n[MAXGRID][MAXGRID], int m[MAXGRID][MAXGRID])
{
  int i, j;

  for (i = 0; i < rows; i++)
  {
    for (j = 0; j < cols; j++)
    {
      if (n[i][j] != m[i][j])
      {
        return 0;
      }
    }
  }
  return 1;
}

int allZeros(int rows, int cols, int m[MAXGRID][MAXGRID])
{
  int i, j;
  for (i = 0; i < rows; i++)
  {
    for (j = 0; j < cols; j++)
    {
      if (m[i][j] == 1)
      {
        return 0;
      }
    }
  }
  return 1;
}

