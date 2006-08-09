/******************************************************************************
IrstLM: IRST Language Model Toolkit
Copyright (C) 2006 Marcello Federico, ITC-irst Trento, Italy

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

******************************************************************************/


#ifndef MF_LMTABLE_H
#define MF_LMTABLE_H

using namespace std;

#define LMTMAXLEV  11

#ifndef  LMTCODESIZE
#define  LMTCODESIZE  (int)3
#endif

#define SHORTSIZE (int)2
#define PTRSIZE   (int)sizeof(char *)
#define INTSIZE   (int)4
#define CHARSIZE  (int)1

#define PROBSIZE  (int)4 //use float  
#define QPROBSIZE (int)1 
#define BOUNDSIZE (int)4

#define UNIGRAM_RESOLUTION 10000000.0

typedef enum {INTERNAL,QINTERNAL,LEAF,QLEAF} LMT_TYPE;
typedef char* node;

typedef enum {LMT_FIND,    //!< search: find an entry
  LMT_ENTER,   //!< search: enter an entry 
  LMT_INIT,    //!< scan: start scan
  LMT_CONT     //!< scan: continue scan
} LMT_ACTION;



//cache memory to store links to bigrams store in the tree
#include "mempool.h"
#include "htable.h"

class bigramcache{
private:
  htable* ht;  
  mempool *mp;
  int maxn;
  int entries;
public:

    bigramcache(int maxentries){
    maxn=maxentries;
    entries=0;
    ht=new htable(maxentries, 2 * sizeof(int),STR,NULL); //load factor   
    mp=new mempool(2 * sizeof(int)+sizeof(char*),maxn/10);
  };
  
  ~bigramcache(){
    ht->stat();//ht->map();
    mp->stat();
    delete ht;delete mp;   
  };
  
  void reset(){
    ht->stat();
    delete ht;delete mp;    
    ht=new htable(maxn/5, 2 * sizeof(int),STR,NULL); //load factor   
    mp=new mempool(2*sizeof(int)+sizeof(char*),maxn/10);
    entries=0;
  }
  
  char* get(int* ngp){       
    char *found=ht->search((char *)ngp,HT_FIND);    
    return found;    
  };
  
  int add(int* ngp,char* link){
   
    char* entry=mp->alloc();
    memcpy(entry,(char*)ngp,sizeof(int) * 2);    
    memcpy(entry + 2 * sizeof(int),(char *)&link,sizeof(char *));
    char *found=ht->search((char *)entry,HT_ENTER);
    assert(found==(char *)entry); //false if key is already insided
    entries++;
    return 1;
  }

  int isfull(){ return (entries >= maxn);};  

};



//disktable or accessing tables stored on disk

class disktable{  
private:
  char* buffer;          //!< buffer of disk table
  int   buffer_size;     //!< size of buffer
  int   entry_size;      //!< size of each single entry in buffer
  long  current_border;  //!< current last available entry in buffer
  long  file_border;     //!< last element in disk table
  long  start_position;  /* */  
public:
    
  disktable(fstream& inp, int buffersize, int entrysize,long max_entries){
       buffer_size=buffersize;
      entry_size=entrysize;
      buffer=new char[(buffer_size+1) * entry_size];
      current_border=0;
      file_border=max_entries;
      start_position=inp.tellp();
    };
  
  ~disktable() {delete [] buffer;};
  
  char* get(fstream& inp,int position){    
    assert(position < file_border);
    
    //you can look back at maximum one position before the first in the buffer!
    assert(position >= (current_border-buffer_size -1));
    
    while (position>=current_border){
      int how_many=(current_border + buffer_size <= file_border?buffer_size:file_border-current_border);
      //store last value in position buffer_size;
      memcpy(buffer + buffer_size * entry_size, buffer+(buffer_size-1) * entry_size, entry_size);
      //read the number of elements
      inp.read(buffer,how_many * entry_size);
      //update curent border
      current_border+=how_many;
    }    
    if (position > (current_border-buffer_size-1) ) //then it is in buffer
      return buffer + (position % buffer_size) * entry_size; 
    else
      if (current_border>buffer_size) //asks for last of previous block
        return buffer + buffer_size * entry_size;
    else return NULL;
  };
  
  void rewind(fstream& inp){
    inp.seekp(start_position);
    current_border=0;      
  }  
  
};

class lmtable{
  
  char*      table[LMTMAXLEV+1]; //storage of all levels
  LMT_TYPE tbltype[LMTMAXLEV+1]; //table type for each levels
  int      cursize[LMTMAXLEV+1]; //current size of levels
  int      maxsize[LMTMAXLEV+1]; //current size of levels
  int*    startpos[LMTMAXLEV+1]; //support vector to store start positions
	
  int               maxlev; //max level of table
  char           info[100]; //information put in the header
  
  //statistics 
  int    totget[LMTMAXLEV+1];
  int    totbsearch;
  
  //probability quantization
  bool      isQtable;
  
  int       NumCenters[LMTMAXLEV+1];
  float*    Pcenters[LMTMAXLEV+1];
  float*    Bcenters[LMTMAXLEV+1];
  
  int     lmt_oov_code;
  int     lmt_oov_size;
  int    backoff_state; 
  //improve access speed
  bigramcache* bicache;
		
public:
    
  dictionary     *dict; // dictionary
  
  lmtable();
  
  ~lmtable(){
    delete bicache;
    for (int i=1;i<=maxlev;i++){
      if (table[i]) delete [] table[i];
      if (isQtable){
        if (Pcenters[i]) delete [] Pcenters[i];
				if (i<maxlev) 
          if (Bcenters[i]) delete [] Bcenters[i];
      }
    }
  }
  
  
	void configure(int n,bool quantized){
		maxlev=n;
		if (n==1)
			tbltype[1]=(quantized?QLEAF:LEAF);
		else{
			for (int i=1;i<n;i++) tbltype[i]=(quantized?QINTERNAL:INTERNAL);
			tbltype[n]=(quantized?QLEAF:LEAF);
    }
	};
	
  int maxlevel(){return maxlev;};
	bool isQuantized(){return isQtable;}
  
  
  void savetxt(const char *filename);
  void savebin(const char *filename);
  void dumplm(fstream& out,ngram ng, int ilev, int elev, int ipos,int epos);
  
  void load(fstream& inp);
  void loadtxt(fstream& inp,const char* header);
  void loadbin(fstream& inp,const char* header);
  
  void loadbinheader(fstream& inp, const char* header);
  void loadbincodebook(fstream& inp,int l);
  
  void lmtable::filter(const char* lmfile);
  void lmtable::filter2(const char* lmfile,int buffMb=512);
  
  void loadcenters(fstream& inp,int Order);
	
  double prob(ngram ng); 
  double lprob(ngram ng); 
  
  void *search(char *tb,LMT_TYPE ndt,int lev,int n,int sz,int *w,
               LMT_ACTION action,char **found=(char **)NULL);
  
  int mybsearch(char *ar, int n, int size, unsigned char *key, int *idx);   
  
  int add(ngram& ng,int prob,int bow);
  void checkbounds(int level);
  
  int get(ngram& ng){return get(ng,ng.size,ng.size);}
  int get(ngram& ng,int n,int lev);
  
  int succscan(ngram& h,ngram& ng,LMT_ACTION action,int lev);
  
  const char *maxsuffptr(ngram ong);
  
  inline int putmem(char* ptr,int value,int offs,int size){
    assert(ptr!=NULL);
    for (int i=0;i<size;i++)
      ptr[offs+i]=(value >> (8 * i)) & 0xff;
    return value;
  };
  
  inline int getmem(char* ptr,int* value,int offs,int size){
    assert(ptr!=NULL);
    *value=ptr[offs] & 0xff;
    for (int i=1;i<size;i++)
      *value= *value | ( ( ptr[offs+i] & 0xff ) << (8 *i));
    return *value;
  };
  
  
  int bo_state(int value=-1){ 
    return (value==-1?backoff_state:backoff_state=value); 
  };
  
  
  int nodesize(LMT_TYPE ndt){
    switch (ndt){
      case INTERNAL:
        return LMTCODESIZE + PROBSIZE + PROBSIZE + BOUNDSIZE;
      case QINTERNAL:
        return LMTCODESIZE + QPROBSIZE + QPROBSIZE + BOUNDSIZE;
      case QLEAF:
        return LMTCODESIZE + QPROBSIZE;      
      case LEAF:
        return LMTCODESIZE + PROBSIZE;      
      default:
        assert(0);
        return 0;
    }
  }  
  
  inline int word(node nd,int value=-1)
  {
    int offset=0;
    
    if (value==-1)
      getmem(nd,&value,offset,LMTCODESIZE);
    else
      putmem(nd,value,offset,LMTCODESIZE);
    
    return value;
  };
  
  inline int prob(node nd,LMT_TYPE ndt, int value=-1)
  {
    int offs=LMTCODESIZE;
    int size=(ndt==QINTERNAL || ndt==QLEAF?QPROBSIZE:PROBSIZE);
    
    if (value==-1)
      getmem(nd,&value,offs,size);
    else
      putmem(nd,value,offs,size);
    
    return value;
  };
  
  
  inline int bow(node nd,LMT_TYPE ndt, int value=-1)
  {
    assert(ndt==INTERNAL || ndt==QINTERNAL);
    int size=(ndt==QINTERNAL?QPROBSIZE:PROBSIZE);
    int offs=LMTCODESIZE+size;
    
    if (value==-1)
      getmem(nd,&value,offs,size);
    else
      putmem(nd,value,offs,size);
    
    return value;
  };
  
  inline int bound(node nd,LMT_TYPE ndt, int value=-1)
  {
    assert(ndt==INTERNAL || ndt==QINTERNAL);
    int offs=LMTCODESIZE+2*(ndt==QINTERNAL?QPROBSIZE:PROBSIZE);
    
    if (value==-1)
      getmem(nd,&value,offs,BOUNDSIZE);
    else
      putmem(nd,value,offs,BOUNDSIZE);
    
    return value;
  };
  
  void stat(int lev=0);
  
};

#endif

