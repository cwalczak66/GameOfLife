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
int numOfThreads = 0;
int valueMax = 0;



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


// functions
void SendMsg(int iTo, struct msg *pMsg);
void RecvMsg(int iFrom, struct msg *pMsg);
void *parentThread(void* index);
void *childThread(void *index);

int main(int argc, char *argv[])
{
    
    if (argc < 3) {
        printf("not enough arguments\n");
    }


    
    
    numOfThreads = atoi(argv[1]);
    valueMax = atoi(argv[2]);

    
    // init mailboxes and semaphores
    for (int i = 0; i < MAXTHREAD+1; i++) {
        mailboxes[i].threadIndex = i;
        sem_init(&mailboxes[i].readSem, 0, 0); // reciving sems start at 0
        sem_init(&mailboxes[i].writeSem, 0, 1); // sending sems start at 1
    }
    

    //init threads
    pthread_create(&mailboxes[0].readerThreadId, NULL, parentThread, (void *)&mailboxes[0].threadIndex);
    for (int i = 1; i <= numOfThreads; i++) {
        pthread_create(&mailboxes[i].readerThreadId, NULL, childThread, (void *)&mailboxes[i].threadIndex); // store thread id in mailbox struct, send index of id
    }
    
   


    // destroy everything
    for (int i = 0; i < numOfThreads; i++) {
        pthread_join(mailboxes[i].readerThreadId, NULL);
    }
    for (int i = 0; i < MAXTHREAD+1; i++) {
        sem_destroy(&mailboxes[i].readSem);
        sem_destroy(&mailboxes[i].writeSem);
    }

    return 0;
}





  
// The function to be executed by child threads
void *childThread(void *index)
{
    int thisThreadIndex = *((int *)index);
    struct msg message;
    int sum = 0;
    
    int type = message.type;

    RecvMsg(thisThreadIndex, &message);

  
    type = message.type;
    int v1 = message.value1;
    int v2 = message.value2;
  

    int msgSender = message.iSender;
    if (type == RANGE) {
        for (int i = v1; i <= v2; i++) {
            sum+=i;
        }
        message.iSender = thisThreadIndex;
        message.type = ALLDONE;
        message.value1 = sum;
        message.value2 = 0;
        SendMsg(msgSender, &message);
    } else {
        
    }

    pthread_exit(NULL);
    
    
}

void *parentThread(void* index) {

    int thisThreadIndex = *((int *)index);
    struct msg message;

    int rangePerThread = (int)(valueMax / numOfThreads);
    int remainder = valueMax%numOfThreads;
    int i, totalSum = 0;
    int threads = numOfThreads;

    message.iSender = thisThreadIndex;
    message.type = RANGE;
    message.value1 = 1;
    message.value2 = rangePerThread;

    for (i = 1; i <= numOfThreads; i++) {
        if (i == numOfThreads) {
            message.value2 += remainder;
            printf("the last value2 is : %d\n", message.value2);
        }
        SendMsg(i, &message);
        message.value1+=rangePerThread;
        message.value2+=rangePerThread;
    }


    while(threads > 0) {
        RecvMsg(0, &message);
        if (message.type == ALLDONE) {
            totalSum += message.value1;
        }

        threads--;
    }

    

    printf("sum of 1 to %d is: %d\n", valueMax, totalSum);

    
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


