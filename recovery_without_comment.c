#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<stdint.h>

int dflag = 0; char devName[1024];
int lflag = 0; char listDir[1024];
int rflag = 0; char recoverFile[1024];
int oflag = 0; char outputFile[1024];

uint32_t entrySize = 0;

struct fat_BS
{
    unsigned char       bootjmp[3];
    unsigned char       oem_name[8];
    unsigned short      bytes_per_sector;
    unsigned char       sectors_per_cluster;
    unsigned short      reserved_sector_count;
    unsigned char       fat_num;
    unsigned short      root_entry_count;
    unsigned short      total_sectors_16;
    unsigned char       media_type;
    unsigned short      table_size_16;
    unsigned short      sectors_per_track;
    unsigned short      head_side_count;
    unsigned int        hidden_sector_count;
    unsigned int        total_sectors_32;
    //extended fat32 stuff
    unsigned int        table_size_32;
    unsigned short      extended_flags;
    unsigned short      fat_version;
    unsigned int        root_cluster;
    unsigned short      fat_info;
    unsigned char       reserved_0[12];
    unsigned char       drive_number;
    unsigned char       reserved_1;
    unsigned char       boot_signature;
    unsigned short      backup_BS_sector;
    unsigned int        volume_id;
    unsigned char       volume_label[11];
    unsigned char       fat_type_label[8];
}__attribute__((packed)) BS;

struct DirEntry
{
    unsigned char name[11];
    unsigned char attr;
    unsigned char res;
    unsigned char crt_time_tenth;
    unsigned short crt_time;
    unsigned short crt_date;
    unsigned short last_access_date;
    unsigned short first_hi;
    unsigned short written_time;
    unsigned short written_date;
    unsigned short first_lo;
    uint32_t filesize;
};

int tokenize_path(char **subDirectory, char *input){
  	int num = 0;
	int i = 0;
	char *token = strtok(input , "/");
  	while( token != NULL ){
    		subDirectory[i] = (char*)malloc(sizeof(char)*strlen(token)+1);
    		strcpy( subDirectory[i] , token);
    		strcat( subDirectory[i], "/");
    		token = strtok( NULL , "/");
    		num++;
    		i++;
  	}
  	return num;
}

/** check whether the file name is LFN
 * return 1 -> yes
 * return 0 -> no
 **/
int isLNF(unsigned char attr){
  	if(attr & 0x1111 == 0x1111)
   		return 1;
  	else
    		return 0;
}

unsigned char* correctName(unsigned char* name, unsigned char attr){
	unsigned char* correctName = (unsigned char *)malloc(12);
	int i;
  	int pos = 0;
	if( name[0] == 0xe5) //0xe5 represents deleted file
		name[0] = '?';
	if( ((attr >> 5) & 1) == 0){//is file
		for( i = 0 ; i < 8 ; i++){
			if(name[i] != 0x20){
				memcpy(correctName+pos, name+i, 1);
				pos++;
			}
	  	}
	  	correctName[pos] = '/';
	}
	else{
	  	for( i = 0 ; i < 8 ; i++){
			if(name[i] != 0x20){
				memcpy(correctName+pos, name+i, 1);
				pos++;
			}
		}
		if(name[8]  != 0x20)
			correctName[pos++] = '.';
		for( i = 8 ; i < 11 ; i++){
			if(name[i] != 0x20){
				memcpy(correctName+pos, name+i, 1);
				pos++;
			}
		}
  	}
  	return correctName;
}

/** print out the usage of program
 **/
void print_usage_of_program(char** s){
	printf("Usage: %s -d [device filename] [other arguments]\n",s[0]);
	printf("-l target\t\tList the target directory\n");
	printf("-r target -o dest\tRecover the target pathname\n");
	exit(-1);
}

/**check the validity of argument
 * return 'l' --> list the directory
 * return 'r' --> recover file
 **/
char checkArgument(int argc, char **argv){
	char opt;
	
	while((opt = getopt(argc,argv,"d:l:r:o:"))!=-1){
		switch(opt){
			case 'd': //found "-d arg"
				if( dflag == 1 || lflag == 1 || rflag == 1 || oflag == 1) // if "-d arg" is not the first 
					print_usage_of_program(argv);
				dflag = 1;
				strcpy(devName, optarg);
				break;
			case 'l': //found "-l arg"
				if( dflag == 0 || lflag == 1 || rflag == 1 || oflag == 1) // if "-l arg" is not the second
					print_usage_of_program(argv);
				lflag = 1;
				strcpy(listDir, optarg);
				break;
			case 'r': //found "-r arg"
				if( dflag == 0 || lflag == 1 || rflag == 1 || oflag == 1) // if "-r arg" is not the second
					print_usage_of_program(argv);
				rflag = 1;
				strcpy(recoverFile, optarg);
				break;
			case 'o': //found -o arg"
				if( dflag == 0 || lflag == 1 || rflag == 0 || oflag == 1) // if "-o arg" is not the third
					print_usage_of_program(argv);
				oflag = 1;
				strcpy(outputFile , optarg);
				break;
			default: //other arg
				print_usage_of_program(argv);
				break;
		}
	}
	if( dflag == 0 ) //missing "-d arg"
		print_usage_of_program(argv);
	else if( lflag == 0 && rflag == 0 && oflag == 0)  //missing "-l arg"
		print_usage_of_program(argv);
	else if( lflag == 0 && rflag == 1 && oflag ==0) //missing "-o arg"
		print_usage_of_program(argv);
	else if( argc > optind) //excessive arg
		print_usage_of_program(argv); 
	else if( dflag == 1 && lflag == 1) //list directory
		return 'l';
	else //recover file
		return 'r';
}

/** initialize Boot Sector of the device
 **/
void init_BS(){
	FILE *in = fopen(devName, "rb"); //rb means read-only for non-text file
	if(in == NULL){
		return;
	}
	fread(&BS, sizeof(BS), 1, in);
	fclose(in);
}

/** check whether the list directory is a sub-directory
 *  return 1 --> yes
 *  return 0 --> no
 **/
int isSubDir(){
	return strcmp(listDir,"/");
}

/** combine the high and low to a complete cluster address
  * return the complete cluster address
  */
unsigned int getClusterAddr(short high, short low){
  	return (unsigned int)(high * 0x10000 + low);
}

/** either list the content of targetCluster with operation = 'l'
  * or 
  * find the targetDir in the targetCluster with operation = 'f'
  * for 'l'
  *   return 0
  * for 'f'
  *   return clusterAddr of targetDir if found
  *   return 0 if not found
  **/
unsigned int accessCluster(FILE* in, unsigned int targetCluster, char operation, char *targetDir){
	unsigned int ROOT_START = (BS.reserved_sector_count + BS.table_size_32 * BS.fat_num) * BS.bytes_per_sector;
  	unsigned int FAT_START = BS.reserved_sector_count* BS.bytes_per_sector;
  	unsigned int CLUSTER_SIZE = BS.bytes_per_sector * BS.sectors_per_cluster;
	
	int i = 0;
	struct DirEntry entry;
	unsigned int data_read = 0;

	while( targetCluster < 0x0ffffff8){ // 0x0ffffff8 represents EOF of cluster
    		while( data_read < CLUSTER_SIZE){ //read one cluster
      			fseek(in, ROOT_START + (targetCluster-2) * CLUSTER_SIZE + data_read, SEEK_SET); // go to the target cluster
      			fread(&entry, sizeof(entry), 1, in);
      			//printentry(entry);	
 		    	  data_read += 32;
      			if( entry.name[0] == 0x00)
        			break;

      			if(isLNF(entry.attr) == 1)
				continue;
      			
            		else if(operation == 'l'){ //do listing the content of targetCluster
        			i++;
				printf("%d, %s, %d, %d\n", i, correctName(entry.name,entry.attr), entry.filesize, getClusterAddr(entry.first_hi, entry.first_lo));
      			}
            		else if(operation == 'f'){ //do finding the targetDir
              			if( strcmp(correctName(entry.name, entry.attr), targetDir) == 0){
                      			entrySize = entry.filesize; //for recovery usage
                      			return getClusterAddr(entry.first_hi, entry.first_lo);
              			}
            		}
    		}
    		data_read = 0;
    		fseek(in, FAT_START + targetCluster * 4, SEEK_SET); // go to fat[cluster]
    		fread(&targetCluster, 4, 1, in);
    		targetCluster &= 0x0FFFFFFF;
	}
  	return 0;
}

/** access the directory which is a sub directory
  * return targetClusterAddr if the sub directory is found
  * return 0 otherwise
  **/
unsigned int access_sub_directory(FILE* in, char** subDirectory, int layer){
  	unsigned int targetCluster = 2; //root-cluster
  	int curLayer;
  	
	for( curLayer = 0 ; curLayer < layer ; curLayer++){
        	targetCluster = accessCluster(in, targetCluster, 'f', subDirectory[curLayer]);
        	if(targetCluster > 0){ //if subDirectory[curLayer] is found
        	  	continue;
        	}
        	else{
        	  	return 0; //cannot find the directory
        	}
  	}
	return targetCluster;
}

/** do the list target directory job
 **/
void list_target_directory(FILE* in){
	if(isSubDir()){ //list sub-directory
	char path[1024];
		strcpy(path, listDir);
		char **subDirectory = (char**)malloc(sizeof(char*)*1024);
		int layer = tokenize_path(subDirectory, path); // tokenize path = "aaa/bbb/ccc" into {"aaa/","bbb/","ccc/"} and store it in subDirectory
		unsigned int targetCluster = access_sub_directory(in, subDirectory, layer);
		if(targetCluster>0) // if the sub_directory is found
			accessCluster(in, targetCluster, 'l', NULL); // list the content of the targetCluster
		else
			printf("error: cannot find the sub-directory=[%s]\n", listDir);
	}
	else{		//list root-directory, 2 is cluster of root-directory
		accessCluster(in, 2, 'l', NULL); //list the content of root-directory
	}
}

/** do the recover target pathname job
 **/
void recover_target_pathname(FILE* in){
  	unsigned int ROOT_START = (BS.reserved_sector_count + BS.table_size_32 * BS.fat_num) * BS.bytes_per_sector;
  	unsigned int FAT_START = BS.reserved_sector_count* BS.bytes_per_sector;
    unsigned int CLUSTER_SIZE = BS.bytes_per_sector * BS.sectors_per_cluster;
   	
   	char path[1024];
   	strcpy(path, recoverFile);
   	char **subDirectory = (char**)malloc(sizeof(char*)*1024);
	int layer = tokenize_path(subDirectory, path); // "aaa/bbb/FILE => {"aaa/", "bbb/", "FILE/"}, layer = 3
	layer --; //layer = 2
	subDirectory[layer][strlen(subDirectory[layer])-1] = '\0'; // {"aaa/", "bbb/", "FILE/"} => {"aaa/", "bbb/", "FILE"}
    	
	char targetName[1024];
	strcpy(targetName, subDirectory[layer]); //targetName = "FILE"
	char fileName[1024];
	strcpy(fileName, targetName); //fileName = "FILE"
	targetName[0] = '?'; //targetName = "?ILE"
	
	int isFound = 0;
	
	unsigned int targetCluster = access_sub_directory(in, subDirectory, layer);
	if(targetCluster > 0){ //the sub-directory is found
		targetCluster = accessCluster(in, targetCluster, 'f', targetName);
      		if(targetCluster > 0){ //the targetName is found
        		isFound = 1; 
      		}
    	}
    	if( isFound == 0){
		printf("[%s]: error - file not found\n", fileName);
		exit(-1);
  	}

	int occupied;
	fseek(in, FAT_START + targetCluster * 4, SEEK_SET);
	fread(&occupied, 4, 1, in);
	occupied &= 0x0FFFFFFF;
 	if( occupied != 0){
		printf("[%s]: error - fail to recover\n", fileName);
		exit(-1);
	}
  	
	unsigned char *buf = (unsigned char*)malloc(sizeof(unsigned char)*CLUSTER_SIZE);
	//  char *buf = (char*)malloc(sizeof(char)*cluster_size);
	//******************check the empty file 
	fseek(in, ROOT_START + (targetCluster-2) * CLUSTER_SIZE, SEEK_SET);
	fread(buf, entrySize, 1, in);
  	
	FILE *fp;
	if((fp = fopen(outputFile, "w+")) == NULL){
		printf("[%s]: failed to open", outputFile);
	}
  	//fwrite(buf, cluster_size, 1, fp);
  	fwrite(buf, entrySize, 1, fp);
  	fclose(fp);
  	printf("[%s]: recovered\n", fileName);
}
	
int main(int argc, char **argv){
	char opt = checkArgument(argc, argv);
	init_BS(); //init the Boot Sector
	FILE* in = fopen(devName,"rb"); //open the device
	if( in == NULL){
		perror("error");
		return 0;
	}
	switch(opt){
		case 'l':
			list_target_directory(in);
			break;
		case 'r':
			recover_target_pathname(in);
			break;
	}
	fclose(in); 
	return 0;
}
