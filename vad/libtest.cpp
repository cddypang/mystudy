#include <iostream>
#include <fstream>
#include <queue>
#include <unistd.h>
#include <thread>
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
	nDataReadLen = fread(*pAudio, 1, *nAudioLen, fs);
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
	nDataReadLen = fread(*pAudio, 1, *nAudioLen, fs);
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

int asrtest(CYVOICE_HANDLE hd, const std::string& wavfile)
{
	if(wavfile.length() < 3)
	{
		printf("invalid filename, support [*.wav *.pcm]\n");
		return -1;
	}
	const std::string& filename = wavfile;
  std::string ext = filename.substr(filename.length()-3, 3);
	char *audio = nullptr;
  int audiolen = 0;
  if(ext == "pcm")
  {
    get_pcm(filename.c_str(), &audio, &audiolen);
  }
  else if (ext == "wav")
  {
    int samp, bit_per_samp;
		short chanel;
    get_wav(filename.c_str(), &audio, &audiolen, &samp, &bit_per_samp, &chanel);
    if(samp != 16000 || bit_per_samp != 16 || chanel != 1)
    {
      printf("unsupport wave format!\n");
			printf("file, sample: %d, bit_per_sample: %d, chanel: %d!\n",
			  samp, bit_per_samp, chanel);
      return -1;
    }
  }
  else
  {
    printf("unknow data format!\n");
    return -1;
  }
  
  if(audio && audiolen > 0)
	  std::cout << "read wave data OK!" << std::endl;
  else
  {
    return -1;
  }
  
  uint16_t statusCode;
  int ifret = cyVoiceStart(hd);
	
	for (int i = 0; i < 1 ; i++)
	{
		{
			int total_len = audiolen, idx = 0, offset = 0, trunk_size = 8000;
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
	      
				if(ifret == CYVOICE_EXIT_WAVE_PIECE_TOO_SHORT)
				{
				  std::cout << "CloudVDPostData put pcm packet error, total_len: " << offset 
				    << ", packet count: " << idx << std::endl;
					break;					
				}

				if(offset >= total_len)
				{
				  std::cout << "CloudVDPostData put pcm packet finish, total_len: " << offset 
				    << ", packet count: " << idx << std::endl;
					break;
				}

				cyVoiceProcessData2(hd);
				ifret = cyVoiceSearchForward(hd, &statusCode);
			}
		}
    if(audio)
    {
      free(audio);
      audio = nullptr;
    }
		cyVoiceProcessData2(hd);
    ifret = cyVoiceStop(hd);
		cyVoiceProcessData2(hd);

    ifret = cyVoiceSearchForward(hd, &statusCode);
		char result_txt[1024] = {0};
		int32_t result_score;
    ifret = cyVoiceQueryResult(hd, &statusCode, result_txt, &result_score);
		if(!ifret)
		{
			printf("cyVoiceQueryResult, result: %s\n", result_txt);
		}
		else
		{
			printf("cyVoiceQueryResult error, return code: %d\n", ifret);
		}
		
	}

	return 0;
}

typedef struct stAsrThreadArg
{
	void* handle;
	std::string wavfile;
} AsrThreadArgT;

void asr_thread(void* args)
{
	AsrThreadArgT* args_ = (AsrThreadArgT*)args;
	if(args_)
	{
		asrtest(args_->handle, args_->wavfile);
	}
}

int main(int argc, char* argv[])
{
	if(argc != 4)
	{
		printf("./a.out config.cfg wavfile engine-cnt\n");
		return 0;
	}
	
  printf("app start\n");
  cyVoiceInit(argv[1]);
  printf("lib init \n");

  int engine_cnt = atoi(argv[3]);
  std::vector<std::thread*> threads;
	std::vector<CYVOICE_HANDLE> engine_hds;

  for(int i= 0; i<engine_cnt; i++)
	{
	  CYVOICE_HANDLE hd = nullptr;
    cyVoiceCreateInstanceEx(&hd);
		engine_hds.push_back(hd);
    printf("thread[%d] lib create finish\n", i);

    AsrThreadArgT arg;
		arg.handle = hd;
		arg.wavfile = argv[2];
		
    std::thread* th = new std::thread(&asr_thread, &arg);		
		threads.push_back(th);
		usleep(20*1000);
	}

  while(fgetc(stdin) != 'q')
	{
    usleep(500*1000);
	}

  for(int i=0; i<threads.size(); i++)
	{
		std::thread* th = threads[i];
		if(th && th->joinable())
		{
			th->join();
			delete th;
			th = nullptr;
		}
		cyVoiceReleaseInstance(engine_hds[i]);
	}
  //asrtest(hd, argv[2]);

  cyVoiceUninit();
  printf("application quit!!!\n");
	return 0;
}

