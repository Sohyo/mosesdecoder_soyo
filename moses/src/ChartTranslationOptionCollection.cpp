// vim:tabstop=2
/***********************************************************************
 Moses - factored phrase-based language decoder
 Copyright (C) 2010 Hieu Hoang

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

#include "util/check.hh"
#include "ChartTranslationOptionCollection.h"
#include "ChartCellCollection.h"
#include "InputType.h"
#include "StaticData.h"
#include "DecodeStep.h"
#include "DummyScoreProducers.h"
#include "Util.h"
#include "RuleMap.h"

using namespace std;

namespace Moses
{

ChartTranslationOptionCollection::ChartTranslationOptionCollection(InputType const& source
    , const TranslationSystem* system
    , const ChartCellCollection &hypoStackColl
    , const std::vector<ChartRuleLookupManager*> &ruleLookupManagers)
  :m_source(source)
  ,m_system(system)
  ,m_decodeGraphList(system->GetDecodeGraphs())
  ,m_hypoStackColl(hypoStackColl)
  ,m_ruleLookupManagers(ruleLookupManagers)
  ,m_translationOptionList(StaticData::Instance().GetRuleLimit())
{
}

ChartTranslationOptionCollection::~ChartTranslationOptionCollection()
{
  RemoveAllInColl(m_unksrcs);
  RemoveAllInColl(m_cacheTargetPhraseCollection);
}

void ChartTranslationOptionCollection::CreateTranslationOptionsForRange(
  const WordsRange &wordsRange)
{

  assert(m_decodeGraphList.size() == m_ruleLookupManagers.size());

  m_translationOptionList.Clear();

  std::vector <DecodeGraph*>::const_iterator iterDecodeGraph;
  std::vector <ChartRuleLookupManager*>::const_iterator iterRuleLookupManagers = m_ruleLookupManagers.begin();
  for (iterDecodeGraph = m_decodeGraphList.begin(); iterDecodeGraph != m_decodeGraphList.end(); ++iterDecodeGraph, ++iterRuleLookupManagers) {
    const DecodeGraph &decodeGraph = **iterDecodeGraph;
    assert(decodeGraph.GetSize() == 1);
    ChartRuleLookupManager &ruleLookupManager = **iterRuleLookupManagers;
    size_t maxSpan = decodeGraph.GetMaxChartSpan();
    if (maxSpan == 0 || wordsRange.GetNumWordsCovered() <= maxSpan) {
      ruleLookupManager.GetChartRuleCollection(wordsRange, m_translationOptionList);
    }

    //FB : loop through translation options and check that they have the same source range
    vector<FactorType> srcFactors;
    srcFactors.push_back(0);

    for (size_t i = 0; i < m_translationOptionList.GetSize(); ++i) {
        const ChartTranslationOption &transOpt = m_translationOptionList.Get(i);
        //all words ranges should be the same, otherwise crash
        CHECK(transOpt.GetSourceWordsRange() == wordsRange);
        float score = 0;

        std::string sourceSide = "";
        std::string targetRepresentation;
        TargetPhraseCollection::const_iterator itr_targets;
        std::vector<TargetPhrase> accumulator;
        //BEWARE : to access the source side and score of a rule, we have to access each target phrase
        //1. Go through each target phrase
        //2. Store all targetPhrases for this source phrase
        //3. Call vw
        //4. Store new score in each targetPhrase
        //5. Re-estimate score of translation option
        for(
            itr_targets = transOpt.GetTargetPhraseCollection().begin();
            itr_targets != transOpt.GetTargetPhraseCollection().end();
            itr_targets++)
        {
            //get lhs of rule
            CHECK((**itr_targets).GetSourcePhrase() != NULL);
            std::cout << "Source Side of rule : " << (*(**itr_targets).GetSourcePhrase()) << std::endl;
            std::cout << "Target Side of rule : " << **itr_targets << std::endl;

            for(int i=0; i<(**itr_targets).GetSourcePhrase()->GetSize();i++)
            {
                sourceSide += (**itr_targets).GetSourcePhrase()->GetWord(i).GetString(srcFactors,0);
                std::cout << "Added string" << sourceSide << std::endl;
            }

            //Append alignments to non-terminals
            std::vector<size_t> ntim = (*itr_targets)->GetAlignmentInfo().GetNonTermIndexMap();
            int nonTermCounter = 0;

            //NonTermCounter should stay smaller than nonTermIndexMap
            for(int i=0; i<(**itr_targets).GetSize();i++)
            {
                CHECK(nonTermCounter < (**itr_targets).GetSize());

                //look for non-terminals
                if((**itr_targets).GetWord(i).IsNonTerminal() == 1)
                {
                    //append non-terminal
                    targetRepresentation += (**itr_targets).GetWord(i).GetString(srcFactors,0);

                    //append alignment
                    stringstream s;
                    s << ntim[nonTermCounter];
                    string alignInd = s.str();
                    targetRepresentation += alignInd;
                    nonTermCounter++;
                }
                else
                {
                    targetRepresentation += (**itr_targets).GetWord(i).GetString(srcFactors,0);
                }
            }
            //accumulate target phrases
            RuleMap ruleMap;
            ruleMap.AddRule(sourceSide,targetRepresentation);
        }
        //map is done, iterate over and call vw
        std::map<std::string,std::string>::const_iterator itr_ruleMap;
        for(itr_ruleMap = RuleMap.begin(); itr_ruleMap != RuleMap.end(); itr_ruleMap++)
        {

        }
    }

  }
//#ifdef HAVE_VW
      // add PSD scores if user specified it
      //PSDScoreProducer *psd = StaticData::Instance().GetPSDScoreProducer();
      //if (psd != NULL) {
        //vector<ScoreComponentCollection> scores = psd->ScoreOptions(partTransOptList, m_source);
        //vector<ScoreComponentCollection>::const_iterator iterPSD = scores.begin();
        //for (iterColl = partTransOptList.begin() ; iterColl != partTransOptList.end() ; ++iterColl) {
        //  assert(iterPSD != scores.end());
        //  (*iterColl)->AddStatelessScore(*iterPSD++);
        //}
      //}
//#endif // HAVE_VW




/*

         //call vw here
  //m_transopt : all to
    foreach lhs
        foreach rhs
        accumulate rhs
        call vw
    foreach rhs
        normalize
    prune
  //ohter loop: normalize
*/


  //prune here

  if (wordsRange.GetNumWordsCovered() == 1 && wordsRange.GetStartPos() != 0 && wordsRange.GetStartPos() != m_source.GetSize()-1) {
    bool alwaysCreateDirectTranslationOption = StaticData::Instance().IsAlwaysCreateDirectTranslationOption();
    if (m_translationOptionList.GetSize() == 0 || alwaysCreateDirectTranslationOption) {
      // create unknown words for 1 word coverage where we don't have any trans options
      const Word &sourceWord = m_source.GetWord(wordsRange.GetStartPos());
      ProcessOneUnknownWord(sourceWord, wordsRange);
    }
  }

  m_translationOptionList.ApplyThreshold();
}

//! special handling of ONE unknown words.
void ChartTranslationOptionCollection::ProcessOneUnknownWord(const Word &sourceWord, const WordsRange &range)
{
  // unknown word, add as trans opt
  const StaticData &staticData = StaticData::Instance();
  const UnknownWordPenaltyProducer *unknownWordPenaltyProducer = m_system->GetUnknownWordPenaltyProducer();
  vector<float> wordPenaltyScore(1, -0.434294482); // TODO what is this number?

  const ChartCell &chartCell = m_hypoStackColl.Get(range);
  const ChartCellLabel &sourceWordLabel = chartCell.GetSourceWordLabel();

  size_t isDigit = 0;
  if (staticData.GetDropUnknown()) {
    const Factor *f = sourceWord[0]; // TODO hack. shouldn't know which factor is surface
    const string &s = f->GetString();
    isDigit = s.find_first_of("0123456789");
    if (isDigit == string::npos)
      isDigit = 0;
    else
      isDigit = 1;
    // modify the starting bitmap
  }

  Phrase* m_unksrc = new Phrase(1);
  m_unksrc->AddWord() = sourceWord;
  m_unksrcs.push_back(m_unksrc);

  //TranslationOption *transOpt;
  if (! staticData.GetDropUnknown() || isDigit) {
    // loop
    const UnknownLHSList &lhsList = staticData.GetUnknownLHS();
    UnknownLHSList::const_iterator iterLHS;
    for (iterLHS = lhsList.begin(); iterLHS != lhsList.end(); ++iterLHS) {
      const string &targetLHSStr = iterLHS->first;
      float prob = iterLHS->second;

      // lhs
      //const Word &sourceLHS = staticData.GetInputDefaultNonTerminal();
      Word targetLHS(true);

      targetLHS.CreateFromString(Output, staticData.GetOutputFactorOrder(), targetLHSStr, true);
      CHECK(targetLHS.GetFactor(0) != NULL);

      // add to dictionary
      TargetPhrase *targetPhrase = new TargetPhrase(Output,*m_unksrc);
      TargetPhraseCollection *tpc = new TargetPhraseCollection;
      tpc->Add(targetPhrase);

      m_cacheTargetPhraseCollection.push_back(tpc);
      Word &targetWord = targetPhrase->AddWord();
      targetWord.CreateUnknownWord(sourceWord);

      // scores
      vector<float> unknownScore(1, FloorScore(TransformScore(prob)));

      //targetPhrase->SetScore();
      targetPhrase->SetScore(unknownWordPenaltyProducer, unknownScore);
      targetPhrase->SetScore(m_system->GetWordPenaltyProducer(), wordPenaltyScore);
       //damt hiero : source phrase built in constructor of target phrase
      //targetPhrase->SetSourcePhrase(m_unksrc);
      targetPhrase->SetTargetLHS(targetLHS);

      // chart rule
      m_translationOptionList.Add(*tpc, m_emptyStackVec, range);
    } // for (iterLHS
  } else {
    // drop source word. create blank trans opt
    vector<float> unknownScore(1, FloorScore(-numeric_limits<float>::infinity()));

    TargetPhrase *targetPhrase = new TargetPhrase(Output,*m_unksrc);
    TargetPhraseCollection *tpc = new TargetPhraseCollection;
    tpc->Add(targetPhrase);
    // loop
    const UnknownLHSList &lhsList = staticData.GetUnknownLHS();
    UnknownLHSList::const_iterator iterLHS;
    for (iterLHS = lhsList.begin(); iterLHS != lhsList.end(); ++iterLHS) {
      const string &targetLHSStr = iterLHS->first;
      //float prob = iterLHS->second;

      Word targetLHS(true);
      targetLHS.CreateFromString(Output, staticData.GetOutputFactorOrder(), targetLHSStr, true);
      CHECK(targetLHS.GetFactor(0) != NULL);

      m_cacheTargetPhraseCollection.push_back(tpc);
      //damt hiero : source phrase built in constructor of target phrase
      //targetPhrase->SetSourcePhrase(m_unksrc);
      targetPhrase->SetScore(unknownWordPenaltyProducer, unknownScore);
      targetPhrase->SetTargetLHS(targetLHS);

      // chart rule
      m_translationOptionList.Add(*tpc, m_emptyStackVec, range);
    }
  }
}

}  // namespace
