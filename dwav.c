/**
 * @file dwav.c
 * @author Nicky Kriplani (Github/NickyDCFP), with very special thanks to Dr. Eric Nelson!
 * 
 * @brief dWAV is a command-line .wav file disassembler. It disassembles and prints the
 *        human-readable portions of a .wav file. Additionally, it can make alterations like
 *        sample rate changes and data reversal, writing the modified data to another file.
 * 
 * @version 1.1
 * @date July 21, 2022
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#define SUBCHUNKIDSIZE 4 //Size of the data subchunk's ID and Size fields
#define FMTSUBCHUNKSIZENOPARAMS 16 //Size of the Format subchunk without any extra parameters
#define DEFAULTINPUTFILENAME "YoshiStoryTheme.wav"
#define DEFAULTOUTPUTFILENAME "ProductFile.wav"
#define VALIDEXTENSION ".wav"
#define MAXEXTRASUBCHUNKS 10 //Number of "extra" subchunks (not riff, fmt, data) dWAV can process
#define NUMVALIDFLAGS 5 
char* VALIDFLAGS[NUMVALIDFLAGS] = {"-i", "-o", "-c", "-hz", "-r"}; //dWAV's supported flags

struct riff { char chunkID[4]; int chunkSize; char format[4]; };
struct fmt { char subChunk1ID[3]; int subChunk1Size; short audioForm, numChannels;
             int sampleRate, byteRate; short blockAlign, bitsPerSample; };
struct data { char subChunk2ID[4]; int subChunk2Size; unsigned char subChunkData[1]; };
struct wav { struct riff riffElements; struct fmt formatElements; struct data dataElements; };
struct extraParams { short extraParamSize; int extraParams[1]; };

bool isValidFlag(char* flag);
void setFilename(char** pFilename, size_t index, int argc, char* argv[]);
bool isValidFilename(char* filename);
void validateSampleRate(size_t index, int argc, char* argv[]);
char* getMemory(char* filename);
size_t getLength(int filehandle);
bool isDataSubChunk(char* subChunk);
void printFile(struct wav* pSoundFile, int extraParamsSize, int numExtraSubChunks,
               struct data extraChunks[]);
void changeSampleRate(struct wav* pSoundFile, int newSampleRate);
void reverseFile(struct wav* pSoundFile);
void writeOutputFile(char* outputfilename, struct wav* pSoundFile, struct extraParams parameters, 
                     int extraParamsSize, int numExtraSubChunks, struct data extraChunks[]);
void changeSpeed(struct wav* pSoundFile, int speedMultiple);
void shiftChannel(struct wav* pSoundFile, int channelLength, int seekArm, int speedMultiple);
bool elimSample(int index, int speedMultiple, int bytesPerSample);

/**
 * @brief Analyzes the flags the user denotes. Opens a .wav file, prints out its data, alters the
 *        data as per the user's specifications, and, if necessary, writes the data to an output
 *        file.
 */
int main(int argc, char* argv[]) {
   char* inputfilename = DEFAULTINPUTFILENAME;
   char* outputfilename = DEFAULTOUTPUTFILENAME;
   //Verify arguments are valid and set requested input and output filenames
   for(size_t i = 1; i < argc; ++i) {
      if(isValidFlag(argv[i])) {
         switch(argv[i][1]) {
            case 'i':
               setFilename(&inputfilename, ++i, argc, argv);
               break;
            case 'o':
               setFilename(&outputfilename, ++i, argc, argv);
               break;
            case 'h':
               validateSampleRate(++i, argc, argv);
               break;
         }
      }
      else {
         printf("%s is not a valid flag. Please consult README for usage.", argv[i]);
         exit(1);
      }
   }
   //Read file and break down into organized structs
   char* wavMem = getMemory(inputfilename);
   unsigned char* wavBytes = (unsigned char*)wavMem;
   int seekArm = sizeof(struct riff) + sizeof(struct fmt);
   int extraParamsSize = (((struct fmt*)&wavBytes[sizeof(struct riff)]))->subChunk1Size - 
                         FMTSUBCHUNKSIZENOPARAMS;
   struct extraParams parameters;
   if(extraParamsSize > 0) {
      parameters = (*(struct extraParams*)&wavBytes[seekArm]);
      seekArm += extraParamsSize;
   }

   //Separate overflow caused by extra subchunks from the data subchunk
   //Store this overflow and reintegrate the data subchunk into the wav struct
   int numExtraSubChunks = 0;
   struct data extraChunks[MAXEXTRASUBCHUNKS];
   while(!isDataSubChunk((char*)&wavBytes[seekArm])) {
      extraChunks[numExtraSubChunks] = (*(struct data*)&wavBytes[seekArm]);
      //increment the seek arm by just the size of the data subchunk's ID and Size
      seekArm += extraChunks[numExtraSubChunks].subChunk2Size + sizeof(char[4]) + sizeof(int);
      ++numExtraSubChunks;
   }
   struct data fileData = *(struct data*)&wavBytes[seekArm];
   struct wav* pSoundFile = (struct wav*)wavBytes;
   pSoundFile->dataElements = fileData;

   printFile(pSoundFile, extraParamsSize, numExtraSubChunks, extraChunks);

   //Execute the remainder of flags and write the result into a new file if necessary
   bool copy = false;
   for(size_t i = 1; i < argc; ++i) {
      switch(argv[i][1]) {
         case 'o':
            copy = true;
         case 'i':
            ++i;
            break;
         case 'c':
            copy = true;
            break;
         case 'h':
            changeSampleRate(pSoundFile, atoi(argv[++i]));
            copy = true;
            break;
         case 'r':
            reverseFile(pSoundFile);
            copy = true;
            break;
      }
   }
   if(copy) {
      writeOutputFile(outputfilename, pSoundFile, parameters, extraParamsSize, numExtraSubChunks, 
                      extraChunks);
   }
   free(wavMem);
}

/**
 * @brief Returns whether or not a flag is valid, that is, if it is contained within the existing
 *        array of valid flags
 * 
 * @param flag the flag to be tested
 * @return true if the existing flag is contained in the array VALIDFLAGS.
 *         false otherwise.
 */
bool isValidFlag(char* flag) {
   //increments through the valid flags and tests for equality to the submitted flag
   for(size_t i = 0; i < NUMVALIDFLAGS; ++i) {
      if(strcmp(flag, VALIDFLAGS[i]) == 0) {
         return true;
      }
   }
   return false;
}

/**
 * @brief Verifies whether the filename the user requested from the command line via the -i or -o 
 *        flag is valid: that is, whether it ends in ".wav". Saves the desired filename.
 * 
 * @param pFilename the pointer which holds the final filename
 * @param index the index in argv at which the desired filename resides
 * @pre pFilename holds one of the default filenames
 * @post pFilename holds the desired filename, assuming that filename was valid
 */
void setFilename(char** pFilename, size_t index, int argc, char* argv[]) {
   if(index >= argc) {
      printf("No filename specified. Please see README for usage.");
      exit(1);
   }
   if(!isValidFilename(argv[index])) {
      printf("Invalid filename %s. Filenames must end with '.wav'.", argv[index]);
      exit(1);
   }
   *pFilename = argv[index];
}

/**
 * @brief Checks to see if a filename is valid.
 * 
 * @param filename the filename whose validity is to be checked.
 * @return true if the filename parameter ends in ".wav".
 *         false otherwise.
 */
bool isValidFilename(char* filename) {
  return (strstr(filename, VALIDEXTENSION) + strlen(VALIDEXTENSION) == filename + strlen(filename));
}

/**
 * @brief Checks to make sure there is a valid (positive and nonzero) sample rate in the
 *        command-line argument following a -hz flag.
 * 
 * @param index the index at which the desired sample rate resides
 */
void validateSampleRate(size_t index, int argc, char* argv[]) {
   if(index >= argc) {
      printf("No sample rate specified. Please see README for usage.");
      exit(1);
   }

   //Tests if the requested sample rate is a positive nonzero integer
   if(atoi(argv[index]) <= 0) {
      printf("Invalid sample rate %s. Sample rates must be positive nonzero integers.", 
             argv[index]);
      exit(1);
   }
}

/**
 * @brief Opens the specified file, allocates enough memory to holds its data, reats the file's
 *        data into that memory, and closes the file.
 * 
 * @param filename the name of the .wav file to be analyzed
 * @return char* a pointer to the memory holding the file data.
 */
char* getMemory(char* filename) {
   //Opens the file
   int filehandle = open(filename, O_RDONLY | O_BINARY);
   if(filehandle == -1) {
      printf("File %s does not exist", filename);
      exit(1);
   }
   printf("Opening file %s\n", filename);

   //Allocates memory for the file
   size_t length = getLength(filehandle);
   char* wavMem = (char*)malloc(length);
   if(!wavMem) {
      printf("Error in allocating memory.");
      exit(1);
   }

   //Reads the file into the memory
   int bytesRead = read(filehandle, wavMem, length);
   if(bytesRead != length) {
      printf("Could not read entire file.");
      exit(1);
   }
   printf("Bytes Read: %d\n", bytesRead);

   close(filehandle);
   return wavMem;
}

/**
 * @brief Finds the length of the file and moves the seek arm back to the beginning of the file.
 * 
 * @param filehandle the handle of the file to be analyzed
 * @return size_t the length of the file in bytes
 */
size_t getLength(int filehandle) {
   size_t currentPos = lseek(filehandle, (size_t)0, SEEK_CUR);
   size_t length = lseek(filehandle, (size_t)0, SEEK_END);
   lseek(filehandle, currentPos, SEEK_SET);
   return length;
}

/**
 * @brief Determines whether the given subchunk is the 'data' subchunk of the sound file.
 * 
 * @param subChunk a pointer to the subchunk to be analyzed
 * @return true if the subchunk's ID is 'data'
           false otherwise
 */
bool isDataSubChunk(char* subChunk) {
   if(strncmp((((struct data*)subChunk)->subChunk2ID), "data", 4) == 0)
      return true;
   return false;
}

/**
 * @brief Prints a formatted summary of the human-readable data in the .wav file
 * 
 * @param pSoundFile a pointer to the .wav file data to be printed
 * @param extraParamsSize the number of extra parameters in the .wav file
 * @param numExtraSubChunks the number of extra subchunks in the .wav file
 * @param extraChunks the extra chunks in the .wav file
 */
void printFile(struct wav* pSoundFile, int extraParamsSize, int numExtraSubChunks, 
               struct data extraChunks[]) { 
   struct riff fileRiff = pSoundFile->riffElements;
   struct fmt fileFormat = pSoundFile->formatElements;
   struct data fileData = pSoundFile->dataElements;

   printf("\nRIFF ELEMENTS\n");
   printf("ChunkID: %c%c%c%c\n", fileRiff.chunkID[0], fileRiff.chunkID[1], fileRiff.chunkID[2], 
                                 fileRiff.chunkID[3]);
   printf("ChunkSize: %d\n", fileRiff.chunkSize);
   printf("Format: %c%c%c%c\n", fileRiff.format[0], fileRiff.format[1], fileRiff.format[2], 
                                fileRiff.format[3]);

   printf("\nFORMAT ELEMENTS\n");
   printf("Subchunk1ID: %c%c%c\n", fileFormat.subChunk1ID[0], fileFormat.subChunk1ID[1], 
                                   fileFormat.subChunk1ID[2]);
   printf("Subchunk1 Size: %d\n", fileFormat.subChunk1Size);
   printf("Audio Form: %d\n", fileFormat.audioForm);
   printf("Number of Channels: %d\n", fileFormat.numChannels);
   printf("Sample Rate: %d\n", fileFormat.sampleRate);
   printf("Byte Rate: %d\n", fileFormat.byteRate);
   printf("Block Align: %d\n", fileFormat.blockAlign);
   printf("Bits Per Sample: %d\n", fileFormat.bitsPerSample);
   if(extraParamsSize > 0) {
      printf("Extra Parameters: Yes\n");
   }
   else {
      printf("Extra Parameters: No\n");
   }
   printf("\nDATA ELEMENTS\n");
   printf("Subchunk2ID: %c%c%c%c\n", fileData.subChunk2ID[0], fileData.subChunk2ID[1], 
                                     fileData.subChunk2ID[2], fileData.subChunk2ID[3]);
   printf("Subchunk2 Size: %d\n", fileData.subChunk2Size);
   printf("\nExtra Subchunks Found: %d \n\n", numExtraSubChunks);
   if(numExtraSubChunks > 0) {
         for(int i = 0; i < numExtraSubChunks; ++i) {
            printf("Extra Subchunk Names: ");
            printf("%.4s", extraChunks[i].subChunk2ID);
            printf(" of Size %d\n", extraChunks[i].subChunk2Size);
         
         if((numExtraSubChunks - i) > 1) {
            printf(", ");
         }
      }
      printf("\n");
   }
}

/**
 * @brief Changes the sample rate of the passed .wav file, altering the byte rate to match it.
 * 
 * @param pSoundFile a pointer to the wav struct whose sample rate is to be changed
 * @param newSampleRate the desired sample rate
 */
void changeSampleRate(struct wav* pSoundFile, int newSampleRate) {
   pSoundFile->formatElements.sampleRate = newSampleRate;
   pSoundFile->formatElements.byteRate = (newSampleRate * pSoundFile->formatElements.blockAlign);
}

/**
 * @brief Reverses the sound data in the passed wav struct
 * 
 * @param pSoundFile the pointer to the wav struct whose data is to be reversed
 */
void reverseFile(struct wav* pSoundFile) {
   int length = pSoundFile->dataElements.subChunk2Size;
   int blockSize = pSoundFile->formatElements.blockAlign;

   unsigned char* temp = (unsigned char*)malloc(pSoundFile->formatElements.blockAlign);
   if(!temp) {
      printf("Error reversing the file.");
      exit(1);
   }
   //Gathers the sample blocks and reverses them individually
   for(int i = 0; i < length / 2; i+= blockSize) {
      for(int j = 0; j < blockSize; j++) {
         temp[j] = pSoundFile->dataElements.subChunkData[i + j];
         pSoundFile->dataElements.subChunkData[i + j] = 
            pSoundFile->dataElements.subChunkData[length - i + j];
         pSoundFile->dataElements.subChunkData[length - i + j] = temp[j];
      }
   }
   free(temp);
}

/**
 * @brief Opens an output file and writes all of the .wav file data to it.
 * 
 * @param outputfilename the filename of the desired output file
 * @param pSoundFile a pointer to the wav struct to be written to the file
 * @param parameters the extra parameters to be written to the file
 * @param extraParamsSize the size of the extra parameters to be written to the file
 * @param numExtraSubChunks the number of extra subchunks to be written to the file
 * @param extraChunks the extra subchunks to be written to the file
 */
void writeOutputFile(char* outputfilename, struct wav* pSoundFile, struct extraParams parameters, 
                     int extraParamsSize, int numExtraSubChunks, struct data extraChunks[]) {
   int outputfilehandle = open(outputfilename, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0644);
   if(outputfilehandle == -1) {
      printf("Error creating or opening output file %s", outputfilename);
      exit(1);
   }
   printf("Writing to file %s\n", outputfilename);
   int bytesWritten = 0;
   //Write riff and format subchunks
   bytesWritten += write(outputfilehandle, (char*)(&(pSoundFile->riffElements)), 
                         sizeof(struct riff));
   bytesWritten += write(outputfilehandle, (char*)(&(pSoundFile->formatElements)), 
                         sizeof(struct fmt));
   //Write extra parameters
   if(extraParamsSize > 0) {
      bytesWritten += write(outputfilehandle, (char*)(&parameters), extraParamsSize);
   }
   //Write extra subchunks
   for(int i = 0; i < numExtraSubChunks; i = i + 1) {
      bytesWritten += write(outputfilehandle, (char*)(&(extraChunks[i])), 
                            extraChunks[i].subChunk2Size + sizeof(char[4]) + sizeof(int));
      //                                                      Data ID         Data Size
   }
   //Write data subchunk
   bytesWritten += write(outputfilehandle, (char*)(&(pSoundFile->dataElements)), 
                        pSoundFile->dataElements.subChunk2Size + sizeof(char[4]) + sizeof(int));
   printf("Bytes Written: %d\n", bytesWritten);
   close(outputfilehandle);
}