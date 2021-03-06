#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#ifndef max
	#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )
#endif


/* Read sequence from a file to a char vector.
Filename is passed as parameter */

struct lcsData {
    MPI_Status * Stat;
    int numtasks;
    int rank;
    int wcluster,hcluster; //cluster matrix width and height
    int icluster,jcluster; // coordinates in cluster matrix
    
    int wchunk, hchunk; // chunk matriz width and height
    int wchunk2, hchunk2; // chunk matriz width and height
    
    // sequence pointers for both sequences
	char *seqA, *seqB;
    
	// sizes of both sequences
	int sizeA, sizeB;
    
    int ** scoreMatrix; //score matrix
    
};

char* read_seq(char *fname){
	//file pointer
	FILE *fseq=NULL;
	//sequence size
	long size=0;
	//sequence pointer
	char *seq=NULL;
	//sequence index
	int i=0;

	//open file
	fseq=fopen(fname,"rt");
	if(fseq==NULL){
		printf("Error reading file %s\n", fname);
		exit(1);
	}

	//find out sequence size to allocate memory afterwards	
	fseek (fseq , 0L , SEEK_END);
	size=ftell(fseq);
	rewind (fseq);

	//allocate memory (sequence)
	seq=(char *) calloc(size+1,sizeof(char));
	if(seq==NULL){
		printf("Erro allocating memory for sequence %s.\n", fname);
		exit(1);
	}

	//read sequence from file
	while(!feof(fseq)){
		seq[i]=fgetc(fseq);
		if ((seq[i]!='\n') && (seq[i]!=EOF))
			i++;
	}
	//insert string terminator
	seq[i]='\0';
	
	//close file
	fclose(fseq);

	//return sequence pointer
	return seq;
}

int ** allocateScoreMatrix(int sizeA, int sizeB)
{
	int i;
	//Allocate memory for LCS score matrix    
    	int ** scoreMatrix = (int **) malloc((sizeB+1)*sizeof(int *));
    	for (i=0; i<(sizeB+1); i++)
		scoreMatrix[i]=(int *) malloc((sizeA+1)*sizeof(int));
	return scoreMatrix;
}


void initScoreMatrix(int ** scoreMatrix, int sizeA, int sizeB)
{
	int i, j;
	//Fill first line of LCS score matrix with zeroes
	for (j=0; j<(sizeA+1); j++)
		scoreMatrix[0][j]=0;

	//Do the same for the first collumn
	for (i=1; i<(sizeB+1); i++)
		scoreMatrix[i][0]=0;
}

int LCS(struct lcsData data)
{
	int i,j;
	for (i=1; i<data.hchunk+1; i++)
	{
		for (j=1;j<data.wchunk+1;j++)
		{
            //printf("%d - compara[%d][%d]\n", data.rank,data.jcluster*data.wchunk+j-1, data.icluster*data.hchunk+i-1);
			if (data.seqA[data.jcluster*data.wchunk2+j-1] == data.seqB[data.icluster*data.hchunk2+i-1])
			{ 
				/* if elements in both sequences match, 
				the corresponding score will be the score from
				previous elements + 1*/
				data.scoreMatrix[i][j] = data.scoreMatrix[i-1][j-1]+1;
			}
			else
			{
				/* else, pick the maximum value (score) from left and upper elements*/
				data.scoreMatrix[i][j] = max(data.scoreMatrix[i-1][j], data.scoreMatrix[i][j-1]);
			}
		}
	}
	return 	data.scoreMatrix[data.hchunk][data.wchunk];
}

void pMatrix(struct lcsData data)
{
	int i,j;
	
	//print header
	printf("Score Matrix %d:\n", data.rank);
	printf("========================================\n");
	
	//print LCS score matrix allong with sequences
	for (i=0; i<data.hchunk+1; i++)
	{
		for (j=0;j<data.wchunk+1;j++)
		{
			printf("%5d   ",data.scoreMatrix[i][j]);
		}
		printf("\n");
	}
	printf("========================================\n");
}

void freeScoreMatrix(int **scoreMatrix, int sizeB)
{
	int i;
    	for (i=0; i<(sizeB+1); i++)
		free(scoreMatrix[i]);
	free(scoreMatrix);
}

void init_ids(struct lcsData * data, char ** argv)
{

    //MPI_Comm_size(MPI_COMM_WORLD, &(data->numtasks));
    //MPI_Comm_rank(MPI_COMM_WORLD, &(data->rank));
    
    data->wcluster = atoi(argv[1]);
    
    data->icluster = data->rank / data->wcluster;
    data->jcluster = data->rank % data->wcluster;
    
    data->hcluster = data->numtasks/data->wcluster;
    
    data->wchunk = (data->sizeA / data->wcluster) + (((data->jcluster+1) == data->wcluster) * (data->sizeA % data->wcluster));
    data->hchunk = (data->sizeB / data->hcluster) + (((data->icluster+1) == data->hcluster) * (data->sizeB % data->hcluster));
    
    data->wchunk2 = (data->sizeA / data->wcluster);
    data->hchunk2 = (data->sizeB / data->hcluster);
    //printf("Rank: %d/%d - Cluster: %d x %d - Chunk: %d x %d\n",data->rank, data->numtasks, data->hcluster, data->wcluster, data->hchunk, data->wchunk);

}

void LCSreceive (struct lcsData data)
{
    int i;
    if (data.icluster > 0)
    {
        //printf("%d <- %d (%d)\n", data.rank, data.rank - data.wcluster, data.wchunk+1);
        MPI_Recv(data.scoreMatrix[0], data.wchunk+1, MPI_INT, data.rank-data.wcluster, 0, MPI_COMM_WORLD, data.Stat);
    }
    else
        for (i=0; i<(data.wchunk+1); i++)
            data.scoreMatrix[0][i]=0;
    if (data.jcluster > 0)
    {
        //printf("%d <- %d (%d)\n", data.rank, data.rank - 1, data.hchunk+1);
        int * fromLeft = (int *) malloc(sizeof(int)*data.hchunk+1);
        MPI_Recv(fromLeft, data.hchunk+1, MPI_INT, data.rank-1, 0, MPI_COMM_WORLD, data.Stat);
        
        for (i=0; i<data.hchunk+1; i++)
            data.scoreMatrix[i][0] = fromLeft[i];
        free(fromLeft);
    }
    else
        for (i=0; i<(data.hchunk+1); i++)
            data.scoreMatrix[i][0]=0;
}

void LCSsend (struct lcsData data)
{
    if ((data.icluster+1) < data.hcluster)
    {
        //printf("%d -> %d (%d)\n", data.rank, data.rank + data.wcluster, data.wchunk+1);
        MPI_Send(data.scoreMatrix[data.hchunk], data.wchunk+1, MPI_INT, data.rank+data.wcluster, 0, MPI_COMM_WORLD);
    }
    if ((data.jcluster+1) < data.wcluster)
    {
        //printf("%d -> %d (%d)\n", data.rank, data.rank + 1, data.hchunk+1);
        int * toRight = (int *) malloc(sizeof(int)*data.hchunk+1);
        int i;
        for (i=0; i<data.hchunk+1; i++)
             toRight[i]=data.scoreMatrix[i][data.wchunk];
        MPI_Send(toRight, data.hchunk+1, MPI_INT, data.rank+1, 0, MPI_COMM_WORLD);
        free(toRight);
    }
}

int main(int argc, char ** argv)
{
    int numtasks, rank;
    //printf("teste");
    
    
    MPI_Status Stat;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &(numtasks));
    MPI_Comm_rank(MPI_COMM_WORLD, &(rank));
    
    struct lcsData data;
    
    data.Stat=&Stat;
    data.rank=rank;
    data.numtasks = numtasks;
    //printf("Files: %s %s\n",argv[2], argv[3]);

	//read both sequences (filenames passed as command line arguments)
	data.seqA = read_seq(argv[2]);
	data.seqB = read_seq(argv[3]);

	//find out sizes
	data.sizeA = strlen(data.seqA);
	data.sizeB = strlen(data.seqB);
	
    init_ids(&data, argv);
    
	// allocate LCS score matrix
	data.scoreMatrix = allocateScoreMatrix(data.wchunk, data.hchunk);
    
    //receive data from neighbors
    LCSreceive(data);
	
		
	//fill up the rest of the matrix and return final score (element locate at the last line and collumn)
	int score = LCS(data);
    //int score=0;
    // send data to neighbors
    LCSsend(data);


    //pMatrix(data);
    
	if (((data.icluster+1) == data.hcluster) && ((data.jcluster+1) == data.wcluster))
    {
        //print score - only the last node
        printf("\nScore: %d\n", score);
    }

	//free score matrix
	freeScoreMatrix(data.scoreMatrix, data.hchunk);
    MPI_Finalize();
    return 0;
}

