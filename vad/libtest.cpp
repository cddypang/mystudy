#include <iostream>
#include <fstream>
#include <queue>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "cyVoice.h"

using namespace std;

int get_wav(const char* pFile, char** pAudio, int* nAudioLen, int* nSampleRate, int* nSampleBits, short* nChannel)
{
	FILE* fs = NULL;
	int nFileSize =0;
	int nHeadLen = 44;
	char buffer[5] = {0};
	int riffSize = 0;
	int formatAttr = 0;
	short compressionCode = 0;
	int bytesPerSecond = 0;
	short blockAlign = 0;
	int nDataReadLen = 0;
	*nSampleRate = 0;
	*nSampleBits = 0;
	*nChannel = 0;
	*pAudio = NULL;
	*nAudioLen = 0;
	//4byte,资源交换文件标志:RIFF
	fs = fopen(pFile, "rb");
	if(fs == NULL)
	{
		return -1;
	}
	fseek(fs, 0L, SEEK_END); 
    nFileSize = ftell(fs);
	if(nFileSize < nHeadLen)//wav头部字节长度是44，
	{
		fclose(fs);
		return -2;
	}
	fseek(fs, 0L, SEEK_SET); 
	//4byte, "RIFF"  0~3
	buffer[0] = '\0';
	fread(buffer, 4, 1, fs);
	if(strcmp(buffer, "RIFF") != 0) 
	{
		fclose(fs);
		return -3;
	}

	//4byte,从下个地址到文件结尾的总字节数  4~7
	riffSize = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8) + ((fgetc(fs) & 255) << 16) + ((fgetc(fs) & 255) << 24);
	//4byte,wave文件标志:"WAVE"  8~11
	memset(buffer, 0, 5);
	fread(buffer, 4, 1, fs);
	if(strcmp(buffer, "WAVE") != 0)
	{
		fclose(fs);
		return -4;
	}
	//4byte,波形文件标志:"FMT "  12~15
	memset(buffer, 0, 5);
	fread(buffer, 4, 1, fs);
	if(strcmp(buffer, "fmt ") != 0)
	{
		fclose(fs);
		return -5;
	}
	//4byte,音频属性 16~19
	formatAttr = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8) + ((fgetc(fs) & 255) << 16) + ((fgetc(fs) & 255) << 24);
	//2byte,编码格式(1-线性pcm-WAVE_FORMAT_PCM,WAVEFORMAT_ADPCM)   20~21
	compressionCode = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8);
	//2byte,通道数 22~23
	*nChannel = (fgetc(fs) & 255) + (fgetc(fs) << 8);
	//4byte,采样率  24~27
	*nSampleRate = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8) + ((fgetc(fs) & 255) << 16) + ((fgetc(fs) & 255) << 24);
	//4byte,传输速率, 其值为通道数×每秒数据位数×每样本的数据位数／8    28~31
	bytesPerSecond = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8) + ((fgetc(fs) & 255) << 16) + ((fgetc(fs) & 255) << 24);
	//2byte,数据块的对齐 ，其值为通道数×每样本的数据位值／8  32~33
	blockAlign = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8);
	//2byte,采样精度 34~35
	*nSampleBits = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8);
	//4byte,数据标志: "data"  36~39
	memset(buffer, 0, 5);
	fread(buffer, 4, 1, fs);
	int nList = 0;
	while(strcmp(buffer, "LIST") == 0)
	{
		nList = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8) + ((fgetc(fs) & 255) << 16) + ((fgetc(fs) & 255) << 24);
		fseek(fs, nList, SEEK_CUR);
		memset(buffer, 0, 5);
		fread(buffer, 4, 1, fs);
	}

	if(strcmp(buffer, "data") != 0)
	{
		fclose(fs);
		return -6;
	}
	//4byte,从下个地址到文件结尾的总字节数，即除了wav header以外的pcm data length  40~43
	*nAudioLen = (fgetc(fs) & 255) + ((fgetc(fs) & 255) << 8) + ((fgetc(fs) & 255) << 16) + ((fgetc(fs) & 255) << 24);
	if(*nAudioLen <= 0)
	{
		fclose(fs);
		return -7;
	}
	*pAudio = (char*)malloc(*nAudioLen * sizeof(char));
	if(*pAudio == NULL)
	{
		fclose(fs);
		return -8;
	}
	fseek(fs, nHeadLen, SEEK_SET);
	nDataReadLen = fread(*pAudio, *nAudioLen, 1, fs);
	if (nDataReadLen != *nAudioLen)
	{
		free(*pAudio);
		*pAudio = NULL;
		fclose(fs);
		return -9;
	}
	fclose(fs);
	return 1;
}

int get_pcm(const char* pFile, char** pAudio, int* nAudioLen)
{
	FILE* fs = fopen(pFile, "rb");
	int nFileSize = 0;
	int nDataReadLen = 0;
	*pAudio = NULL;
	*nAudioLen = 0;	
	if(fs == NULL)
	{
		return -1;
	}
	fseek(fs, 0L, SEEK_END); 
    nFileSize = ftell(fs);
	if(nFileSize <= 0)//空文件
	{
		fclose(fs);
		return -2;
	}
	*nAudioLen = nFileSize;
	*pAudio = (char*)malloc(*nAudioLen);
	if(*pAudio == NULL)
	{
		fclose(fs);
		return -3;
	}
	fseek(fs, 0L, SEEK_SET);
	nDataReadLen = fread(*pAudio, *nAudioLen, 1, fs);
	if (nDataReadLen != *nAudioLen)
	{
		free(*pAudio);
		*pAudio = NULL;
		fclose(fs);
		return -4;
	}
	fclose(fs);
	return 1;
}

int asrtest(CYVOICE_HANDLE hd)
{
	string filename = "./TestData/02FAC9DA80594A310FA1D2B961F8C118-1.wav";
  std::string ext = filename.substr(filename.length()-4, 3);
	char *audio = nullptr;
  int audiolen = 0;
  if(ext == "pcm")
  {
    get_pcm(filename.c_str(), &audio, &audiolen);
  }
  else if (ext == "wav")
  {
    int samp, bit_per_samp, chanel;
    get_wav(filename.c_str(), &audio, &audiolen, &samp, &bit_per_samp, (short*)&chanel);
    if(samp != 16000 || bit_per_samp != 2 || chanel != 1)
    {
      printf("unsupport wave format!\n");
      return -1;
    }
  }
  else
  {
    printf("unknow data format!\n");
    return -1;
  }
  
  if(audio && audiolen > 0)
	  std::cout << "read pcm file OK!" << std::endl;
  else
  {
    return -1;
  }
  

  int ifret = cyVoiceStart(hd);
	
	for (int i = 0; i < 1 ; i++)
	{
		{
			int total_len = audiolen, idx = 0, offset = 44, trunk_size = 8000;
			uint32_t send_len = 0;
			while(1)
			{
			  if((total_len - offset) >= trunk_size)
			    send_len = trunk_size;
			  else
			  {	
				  send_len = total_len - offset;
	      }

				ifret = cyVoiceProcessData1(hd, audio + offset, &send_len, nullptr, nullptr, nullptr);
				offset += send_len;
				usleep(50*1000);
				//std::cout << "CloudVDPostData return: " << ifret << std::endl;
				idx++;
				//std::cout << "CloudVDPostData put pcm packet count: " << idx << std::endl;
				printf("cyVoiceProcessData1[%d] return: %d\n", idx, ifret);
	
				if(offset >= total_len)
				{
				  std::cout << "CloudVDPostData put pcm packet finish, total_len: " << offset 
				    << ", packet count: " << idx << std::endl;
					break;
				}
			}
		}
    if(audio)
    {
      free(audio);
      audio = nullptr;
    }

    ifret = cyVoiceStop(hd);

    ifret = cyVoiceQueryResult(hd, nullptr, nullptr, nullptr);
	}

	return 0;
}

int main(int argc, char* argv[])
{
  printf("app start\n");
  char cfg[64] = "./config.ini";
  cyVoiceInit(cfg);
  printf("lib init \n");

  CYVOICE_HANDLE hd = nullptr;
  cyVoiceCreateInstanceEx(&hd);
  printf("lib create finish\n");

  asrtest(hd);

  cyVoiceReleaseInstance(hd);
  cyVoiceUninit();
  printf("application quit!!!\n");
	return 0;
}

