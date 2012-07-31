/***********************************************************************
  Moses - factored phrase-based language decoder
  Copyright (C) 2011 University of Edinburgh

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
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ***********************************************************************/

#pragma once
#ifndef moses_OutputCollector_h
#define moses_OutputCollector_h

#ifdef WITH_THREADS
#include <boost/thread/mutex.hpp>
#endif

#ifdef BOOST_HAS_PTHREADS
#include <pthread.h>
#endif

#include <iostream>
#include <map>
#include <ostream>
#include <string>

namespace Moses
{
/**
* Makes sure output goes in the correct order when multi-threading
**/
class OutputCollector
{
public:
  OutputCollector(std::ostream* outStream= &std::cout, std::ostream* debugStream=&std::cerr) :
    m_nextOutput(0),m_outStream(outStream),m_debugStream(debugStream),m_sortOutput(true),m_outputId(false)  {}


  /**
    * Write or cache the output, as appropriate.
    **/
  void Write(const int sourceId,const std::string& output,const std::string& debug="") {
	  Write(0, sourceId, output, debug);
  }

  void Write(const int translationId, const int sourceId,const std::string& output,const std::string& debug="") {
#ifdef WITH_THREADS
    boost::mutex::scoped_lock lock(m_mutex);
#endif
    if (sourceId == m_nextOutput || !m_sortOutput) {
      //This is the one we were expecting
      if (m_outputId) *m_outStream << translationId << " ";
      *m_outStream << output << std::flush;
      *m_debugStream << debug << std::flush;
      ++m_nextOutput;
      //see if there's any more
      std::map<int,std::string>::iterator iter;
      while ((iter = m_outputs.find(m_nextOutput)) != m_outputs.end()) {
        if (m_outputId) {
            std::map<int, long>::iterator tidIter = m_translationIds.find(iter->first);
            if (tidIter != m_translationIds.end()) {
                *m_outStream << tidIter->second << " ";
                m_translationIds.erase(tidIter);
            }
        }
        *m_outStream << iter->second << std::flush;
        m_outputs.erase(iter);
        ++m_nextOutput;
        std::map<int,std::string>::iterator debugIter = m_debugs.find(iter->first);
        if (debugIter != m_debugs.end()) {
          *m_debugStream << debugIter->second << std::flush;
          m_debugs.erase(debugIter);
        }
      }
    } else {
      //save for later
      m_outputs[sourceId] = output;
      m_debugs[sourceId] = debug;
      m_translationIds[sourceId] = translationId;
    }
  }

  void setSortOutput(const bool sortOutput) {
    m_sortOutput = sortOutput;
  }
  void setOutputId(const bool outputId) {
    m_outputId = outputId;
  }

private:
  std::map<int,std::string> m_outputs;
  std::map<int,std::string> m_debugs;
  std::map<int,long> m_translationIds;
  int m_nextOutput;
  std::ostream* m_outStream;
  std::ostream* m_debugStream;
  bool m_sortOutput;
  bool m_outputId;
#ifdef WITH_THREADS
  boost::mutex m_mutex;
#endif
};

}  // namespace Moses

#endif
