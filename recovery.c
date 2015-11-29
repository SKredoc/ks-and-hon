#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<stdint.h>

int dflag = 0; char devName[256];
int lflag = 0; char listDir[256];
int rflag = 0; char recoverFile[256];
int oflag = 0; char outputFile[256];

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

/** print out the information of the boot sector
 **/
void printBSinfo(){
    printf("boot sector: jmpboot: %s \n \
        oem_name: %s\n \
        bytes_per_sector: %d\n \
        sectors_per_cluster: %d\n \
        reserved_sector_count: %d\n \
        fat_num: %d\n \
        root_entry_count: %d\n \
        total_sectors_16: %d\n \
        media_type: %c\n \
        table_size_16: %d\n \
        sectors_per_track: %d\n \
        head_side_count: %d\n \
        hidden_sector_count: %d\n \
        total_sectors_32: %d\n \
        table_size_32: %d\n \
        extended_flags: %d\n \
        fat_version: %d\n \
        root_cluster: %d\n \
        fat_info: %d\n \
        backup_BS_sector: %d\n \
        reserved_0: %s\n \
        drive_number: %d\n \
        reserved_1: %d\n \
        boot_signature: %d\n \
        volume_id: %d\n \
        volume_label: %s\n \
        fat_type_label: %s\n", BS.bootjmp, BS.oem_name, BS.bytes_per_sector, BS.sectors_per_cluster,
        BS.reserved_sector_count, BS.fat_num, BS.root_entry_count, BS.total_sectors_16, BS.media_type,
        BS.table_size_16, BS.sectors_per_track, BS.head_side_count, BS.hidden_sector_count, BS.total_sectors_32,
        BS.table_size_32, BS.extended_flags, BS.fat_version, BS.root_cluster, BS.fat_info, BS.backup_BS_sector,
        BS.reserved_0, BS.drive_number, BS.reserved_1, BS.boot_signature, BS.volume_id, BS.volume_label, BS.fat_type_label);

	unsigned int ROOT_START = (BS.reserved_sector_count + BS.table_size_32 * BS.fat_num) * BS.bytes_per_sector;
    unsigned int FAT_START = BS.reserved_sector_count* BS.bytes_per_sector;
    unsigned int cluster_size = BS.bytes_per_sector * BS.sectors_per_cluster;
    unsigned int file_size = BS.bytes_per_sector * BS.total_sectors_32;
    printf("ROOT_START: %d\n", ROOT_START);
    printf("FAT_START: %d\n", FAT_START );
    printf("cluster_size: %d\n", cluster_size);
    printf("file_size; %d\n", file_size);
}

/** print out the information of the directory entry
 **/
void printentry(struct DirEntry entry){
	printf("file entry: name %s\n\
 	attr %c\n\
 	res %c\n\
 	crt_time_tenth %c\n\
 	crt_time %d\n\
    	crt_date %d\n\
    	last_access_date %d\n\
    	first_hi %d\n\
    	written_time %d\n\
    	written_date %d\n\
  	first_lo %d\n\
  	filesize %d\n",entry.name,entry.attr,entry.res,entry.crt_time_tenth,entry.crt_time,
  	entry.crt_date,entry.last_access_date,entry.first_hi,entry.written_time,
  	entry.written_date,entry.first_lo,entry.filesize);
}

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

int tokenize_recover_path(char **subDirectory, char *input){
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
  	i--;
  	subDirectory[i][strlen(subDirectory[i])-1] = '\0';
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
		printf("getopt return:\n\topt = %c\n\toptarg = %s\n", opt,optarg);
		switch(opt){
			case 'd': //found "-d arg"
				if( dflag == 1 || lflag == 1 || rflag == 1 || oflag == 1) // if "-d arg" is not the first 
					print_usage_of_program(argv);
				dflag = 1;
				strcpy(devName, optarg);
				printf("devName = %s\n", devName);
				break;
			case 'l': //found "-l arg"
				if( dflag == 0 || lflag == 1 || rflag == 1 || oflag == 1) // if "-l arg" is not the second
					print_usage_of_program(argv);
				lflag = 1;
				strcpy(listDir, optarg);
				printf("listDir = %s\n", listDir);
				break;
			case 'r': //found "-r arg"
				if( dflag == 0 || lflag == 1 || rflag == 1 || oflag == 1) // if "-r arg" is not the second
					print_usage_of_program(argv);
				rflag = 1;
				strcpy(recoverFile, optarg);
				printf("recoverFile = %s\n", recoverFile);
				break;
			case 'o': //found -o arg"
				if( dflag == 0 || lflag == 1 || rflag == 0 || oflag == 1) // if "-o arg" is not the third
					print_usage_of_program(argv);
				oflag = 1;
				strcpy(outputFile , optarg);
				printf("ouputFile = %s\n", outputFile);
				break;
			default: //other arg
				print_usage_of_program(argv);
				break;
		}
	}
	printf("get return:\n\toptind = %d\n",optind);
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
		perror("error");
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

int firstCluster(short high, short low){
  	return (int)high * 256 + low;
}

void list_cluster(FILE* in, int cluster){
	unsigned int ROOT_START = (BS.reserved_sector_count + BS.table_size_32 * BS.fat_num) * BS.bytes_per_sector;
  	unsigned int FAT_START = BS.reserved_sector_count* BS.bytes_per_sector;
  	unsigned int CLUSTER_SIZE = BS.bytes_per_sector * BS.sectors_per_cluster;
	
	int i = 0;
	printf("cluster = %d\n", cluster);
	struct DirEntry entry;
	unsigned int data_read = 0;

	while( cluster < 0x0ffffff8){ // >=0x0ffffff8 represents EOF of cluster
    		while( data_read < CLUSTER_SIZE){
      			fseek(in, ROOT_START + (cluster-2) * CLUSTER_SIZE + data_read, SEEK_SET); // go to the target cluster
      			fread(&entry, sizeof(entry), 1, in);
      			//printentry(entry);	
 		    	data_read += 32;
      			if( entry.name[0] == 0x00)
        			break;
      			if(isLNF(entry.attr) == 1)
				continue;
      			else{
        			i++;
				printf("%d, %s, %d, %d\n", i, correctName(entry.name,entry.attr), entry.filesize, firstCluster(entry.first_hi, entry.first_lo));
      			}
    		}
    		data_read = 0;
    		// go to fat[cluster]
    		fseek(in, FAT_START + cluster * 4, SEEK_SET);
    		fread(&cluster, 4, 1, in);
    		cluster &= 0x0FFFFFFF;
	}
}

/** list the directory which is a sub directory
 **/
void list_sub_directory(FILE* in){
	printf("list the sub directory start\n");
	unsigned int ROOT_START = (BS.reserved_sector_count + BS.table_size_32 * BS.fat_num) * BS.bytes_per_sector;
  	unsigned int FAT_START = BS.reserved_sector_count* BS.bytes_per_sector;
	unsigned int CLUSTER_SIZE = BS.bytes_per_sector * BS.sectors_per_cluster;

	struct DirEntry entry;
  	int cluster = 2; //root-cluster
  	char path[1024];
  	strcpy(path, listDir);
  	char **subDirectory = (char**)malloc(sizeof(char*)*1024);
  	int layer = tokenize_path(subDirectory, path);
  	unsigned int data_read = 0;
  	int curLayer;
  	int isFound;
  	
	for( curLayer = 0 ; curLayer < layer ; curLayer++){
    		printf("*****find: %s\n", subDirectory[curLayer]);
    		isFound = 0;
    		while( cluster < 0x0ffffff8){ // >=0x0ffffff8 represents EOF of cluster
      			while( data_read < CLUSTER_SIZE){
        			fseek(in, ROOT_START + (cluster-2) * CLUSTER_SIZE + data_read, SEEK_SET);
        			fread(&entry, sizeof(entry), 1, in);
        			//printentry(entry);
        			data_read += 32;
        			if( entry.name[0] == 0x00)
         				break;
        			//printf("name: %s\n",entry.name);
        			if(isLNF(entry.attr) == 1);
          			//printf("it is long file name\n");
        			else{
        				//  printf("Name: %s\n",correctName(entry.name, entry.attr));
          				if( strcmp(correctName(entry.name, entry.attr), subDirectory[curLayer]) == 0){
            					isFound = 1;
            					cluster = firstCluster(entry.first_hi, entry.first_lo);
            					printf("%s is found\n", subDirectory[curLayer]);
            					break;
          				}
        			}
      			}
    	  		data_read = 0;
      			if(isFound)
      	 	 		break;
      			else{
       		 		// go to fat[cluster]
        			fseek(in, FAT_START + cluster * 4, SEEK_SET);
        			fread(&cluster, 4, 1, in);
        			cluster &= 0x0FFFFFFF;
      			}

    		}
  	}
	if(isFound)
  		list_cluster(in, cluster);
	else
		printf("Cannot find the subdirectory\n");
}

/** do the list target directory job
 **/
void list_target_directory(){
	FILE* in = fopen(devName,"rb");
	if( in == NULL){
		perror("error");
		return;
	}

	if(isSubDir()){ //list sub-directory
		list_sub_directory(in);
		fclose(in);
		return;
	}
	else{		//list root-directory, 2 is cluster of root-directory
		list_cluster(in, 2);
		fclose(in);
		return;
	}
	
}

/** do the recover target pathname job
 **/
void recover_target_pathname(){
	printf("recover the target pathname start\n");
	unsigned int ROOT_START = (BS.reserved_sector_count + BS.table_size_32 * BS.fat_num) * BS.bytes_per_sector;
 	unsigned int FAT_START = BS.reserved_sector_count* BS.bytes_per_sector;
  	unsigned int cluster_size = BS.bytes_per_sector * BS.sectors_per_cluster;
  	unsigned int file_size = BS.bytes_per_sector * BS.total_sectors_32;
  	struct DirEntry entry;
	FILE* in = fopen(devName,"rb");
	if( in == NULL){
		perror("error");
		return;
	}
  	int cluster = 2; //root-cluster
  	char path[1024];
  	strcpy(path, recoverFile);
  	char **subDirectory = (char**)malloc(sizeof(char*)*1024);
  	int layer = tokenize_recover_path(subDirectory, path);
  	char targetName[1024];
  	strcpy(targetName, subDirectory[layer-1]);
  	subDirectory[layer-1][0] = '?';
  	unsigned int data_start = ROOT_START;
  	unsigned int data_read = 0;
  	int curLayer;
  	int isFound;
  	int recoverisFound = 0;
  	for( curLayer = 0 ; curLayer < layer ; curLayer++){
    		printf("*****find: %s\n", subDirectory[curLayer]);
		isFound = 0;
	    	while( cluster < 0x0ffffff8){
	      		while( data_read < cluster_size){
        			fseek(in, data_start + (cluster-2) * cluster_size + data_read, SEEK_SET);
        			fread(&entry, sizeof(entry), 1, in);
        			//printentry(entry);
        			data_read += 32;
        			if( entry.name[0] == 0x00)
          				break;
        			//printf("name: %s\n",entry.name);
        			if(isLNF(entry.attr) == 1);
          			//printf("it is long file name\n");
        			else{
        				//  printf("Name: %s\n",correctName(entry.name, entry.attr));
          				if( strcmp(correctName(entry.name, entry.attr), subDirectory[curLayer]) == 0){
            					isFound = 1;
            				if(curLayer == layer-1)
              					recoverisFound = 1;
            					cluster = firstCluster(entry.first_hi, entry.first_lo);
            					printf("%s is found\n", subDirectory[curLayer]);
            					break;
        	  			}
        			}
      			}
      			data_read = 0;
      			if( isFound == 1)
        			break;
     	 		else if( recoverisFound == 1)
       				break;
      			else{
       	 			// go to fat[cluster]
       	 			fseek(in, FAT_START + cluster * 4, SEEK_SET);
      		  		fread(&cluster, 4, 1, in);
        			cluster &= 0x0FFFFFFF;
      			}
    		}
  	}
	 if( recoverisFound == 0){
	    	printf("[%s]: error - file not found\n", targetName);
	    	exit(-1);
  	}
 	int occupied;
 	fseek(in, FAT_START + cluster * 4, SEEK_SET);
 	fread(&occupied, 4, 1, in);
 	occupied &= 0x0FFFFFFF;
 	if( occupied != 0){
    		printf("[%s]: error - fail to recover\n", targetName);
    		exit(-1);
  	}
  	printf("my name: %s\n", correctName(entry.name, entry.attr));
	printf("my cluster: %d\n", cluster);
	unsigned char *buf = (unsigned char*)malloc(sizeof(unsigned char)*cluster_size);
	//  char *buf = (char*)malloc(sizeof(char)*cluster_size);
	//******************check the empty file 
	fseek(in, ROOT_START + (cluster-2) * cluster_size, SEEK_SET);
	fread(buf, entry.filesize, 1, in);
	//fread(buf, cluster_size, 1, in);
	printf("the content\n");
	printf("%s\n", buf);
	
  	FILE *fp;
	if((fp = fopen(outputFile, "w+")) == NULL){
		printf("[%s]: failed to open", outputFile);
	}
	//fwrite(buf, cluster_size, 1, fp);
	fwrite(buf, entry.filesize, 1, fp);
	fclose(fp);
	printf("[%s]: recovered\n", targetName);
}
	
int main(int argc, char **argv){
	char opt = checkArgument(argc, argv);
	init_BS();
	switch(opt){
		case 'l':
			list_target_directory();
			break;
		case 'r':
			recover_target_pathname();
			break;
	}
	return 0;
}
