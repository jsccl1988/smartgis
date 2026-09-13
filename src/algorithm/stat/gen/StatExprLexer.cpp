
// Generated from src/algorithm/stat/StatExpr.g4 by ANTLR 4.13.2


#include "algorithm/stat/gen/StatExprLexer.h"


using namespace antlr4;

using namespace smtstat;


using namespace antlr4;

namespace {

struct StatExprLexerStaticData final {
  StatExprLexerStaticData(std::vector<std::string> ruleNames,
                          std::vector<std::string> channelNames,
                          std::vector<std::string> modeNames,
                          std::vector<std::string> literalNames,
                          std::vector<std::string> symbolicNames)
      : ruleNames(std::move(ruleNames)), channelNames(std::move(channelNames)),
        modeNames(std::move(modeNames)), literalNames(std::move(literalNames)),
        symbolicNames(std::move(symbolicNames)),
        vocabulary(this->literalNames, this->symbolicNames) {}

  StatExprLexerStaticData(const StatExprLexerStaticData&) = delete;
  StatExprLexerStaticData(StatExprLexerStaticData&&) = delete;
  StatExprLexerStaticData& operator=(const StatExprLexerStaticData&) = delete;
  StatExprLexerStaticData& operator=(StatExprLexerStaticData&&) = delete;

  std::vector<antlr4::dfa::DFA> decisionToDFA;
  antlr4::atn::PredictionContextCache sharedContextCache;
  const std::vector<std::string> ruleNames;
  const std::vector<std::string> channelNames;
  const std::vector<std::string> modeNames;
  const std::vector<std::string> literalNames;
  const std::vector<std::string> symbolicNames;
  const antlr4::dfa::Vocabulary vocabulary;
  antlr4::atn::SerializedATNView serializedATN;
  std::unique_ptr<antlr4::atn::ATN> atn;
};

::antlr4::internal::OnceFlag statexprlexerLexerOnceFlag;
#if ANTLR4_USE_THREAD_LOCAL_CACHE
static thread_local
#endif
std::unique_ptr<StatExprLexerStaticData> statexprlexerLexerStaticData = nullptr;

void statexprlexerLexerInitialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  if (statexprlexerLexerStaticData != nullptr) {
    return;
  }
#else
  assert(statexprlexerLexerStaticData == nullptr);
#endif
  auto staticData = std::make_unique<StatExprLexerStaticData>(
    std::vector<std::string>{
      "FIELD", "IDENT", "NUMBER", "EQ", "PLUS", "MINUS", "STAR", "SLASH", 
      "POW", "LPAREN", "RPAREN", "LBRACE", "RBRACE", "COMMA", "WS"
    },
    std::vector<std::string>{
      "DEFAULT_TOKEN_CHANNEL", "HIDDEN"
    },
    std::vector<std::string>{
      "DEFAULT_MODE"
    },
    std::vector<std::string>{
      "", "", "", "", "'='", "'+'", "'-'", "'*'", "'/'", "'^'", "'('", "')'", 
      "'{'", "'}'", "','"
    },
    std::vector<std::string>{
      "", "FIELD", "IDENT", "NUMBER", "EQ", "PLUS", "MINUS", "STAR", "SLASH", 
      "POW", "LPAREN", "RPAREN", "LBRACE", "RBRACE", "COMMA", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,0,15,99,6,-1,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,
  	6,2,7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,
  	7,14,1,0,1,0,4,0,34,8,0,11,0,12,0,35,1,0,1,0,1,1,1,1,5,1,42,8,1,10,1,
  	12,1,45,9,1,1,2,4,2,48,8,2,11,2,12,2,49,1,2,1,2,4,2,54,8,2,11,2,12,2,
  	55,3,2,58,8,2,1,2,1,2,3,2,62,8,2,1,2,4,2,65,8,2,11,2,12,2,66,3,2,69,8,
  	2,1,3,1,3,1,4,1,4,1,5,1,5,1,6,1,6,1,7,1,7,1,8,1,8,1,9,1,9,1,10,1,10,1,
  	11,1,11,1,12,1,12,1,13,1,13,1,14,4,14,94,8,14,11,14,12,14,95,1,14,1,14,
  	0,0,15,1,1,3,2,5,3,7,4,9,5,11,6,13,7,15,8,17,9,19,10,21,11,23,12,25,13,
  	27,14,29,15,1,0,7,1,0,93,93,3,0,65,90,95,95,97,122,4,0,48,57,65,90,95,
  	95,97,122,1,0,48,57,2,0,69,69,101,101,2,0,43,43,45,45,3,0,9,10,13,13,
  	32,32,107,0,1,1,0,0,0,0,3,1,0,0,0,0,5,1,0,0,0,0,7,1,0,0,0,0,9,1,0,0,0,
  	0,11,1,0,0,0,0,13,1,0,0,0,0,15,1,0,0,0,0,17,1,0,0,0,0,19,1,0,0,0,0,21,
  	1,0,0,0,0,23,1,0,0,0,0,25,1,0,0,0,0,27,1,0,0,0,0,29,1,0,0,0,1,31,1,0,
  	0,0,3,39,1,0,0,0,5,47,1,0,0,0,7,70,1,0,0,0,9,72,1,0,0,0,11,74,1,0,0,0,
  	13,76,1,0,0,0,15,78,1,0,0,0,17,80,1,0,0,0,19,82,1,0,0,0,21,84,1,0,0,0,
  	23,86,1,0,0,0,25,88,1,0,0,0,27,90,1,0,0,0,29,93,1,0,0,0,31,33,5,91,0,
  	0,32,34,8,0,0,0,33,32,1,0,0,0,34,35,1,0,0,0,35,33,1,0,0,0,35,36,1,0,0,
  	0,36,37,1,0,0,0,37,38,5,93,0,0,38,2,1,0,0,0,39,43,7,1,0,0,40,42,7,2,0,
  	0,41,40,1,0,0,0,42,45,1,0,0,0,43,41,1,0,0,0,43,44,1,0,0,0,44,4,1,0,0,
  	0,45,43,1,0,0,0,46,48,7,3,0,0,47,46,1,0,0,0,48,49,1,0,0,0,49,47,1,0,0,
  	0,49,50,1,0,0,0,50,57,1,0,0,0,51,53,5,46,0,0,52,54,7,3,0,0,53,52,1,0,
  	0,0,54,55,1,0,0,0,55,53,1,0,0,0,55,56,1,0,0,0,56,58,1,0,0,0,57,51,1,0,
  	0,0,57,58,1,0,0,0,58,68,1,0,0,0,59,61,7,4,0,0,60,62,7,5,0,0,61,60,1,0,
  	0,0,61,62,1,0,0,0,62,64,1,0,0,0,63,65,7,3,0,0,64,63,1,0,0,0,65,66,1,0,
  	0,0,66,64,1,0,0,0,66,67,1,0,0,0,67,69,1,0,0,0,68,59,1,0,0,0,68,69,1,0,
  	0,0,69,6,1,0,0,0,70,71,5,61,0,0,71,8,1,0,0,0,72,73,5,43,0,0,73,10,1,0,
  	0,0,74,75,5,45,0,0,75,12,1,0,0,0,76,77,5,42,0,0,77,14,1,0,0,0,78,79,5,
  	47,0,0,79,16,1,0,0,0,80,81,5,94,0,0,81,18,1,0,0,0,82,83,5,40,0,0,83,20,
  	1,0,0,0,84,85,5,41,0,0,85,22,1,0,0,0,86,87,5,123,0,0,87,24,1,0,0,0,88,
  	89,5,125,0,0,89,26,1,0,0,0,90,91,5,44,0,0,91,28,1,0,0,0,92,94,7,6,0,0,
  	93,92,1,0,0,0,94,95,1,0,0,0,95,93,1,0,0,0,95,96,1,0,0,0,96,97,1,0,0,0,
  	97,98,6,14,0,0,98,30,1,0,0,0,10,0,35,43,49,55,57,61,66,68,95,1,6,0,0
  };
  staticData->serializedATN = antlr4::atn::SerializedATNView(serializedATNSegment, sizeof(serializedATNSegment) / sizeof(serializedATNSegment[0]));

  antlr4::atn::ATNDeserializer deserializer;
  staticData->atn = deserializer.deserialize(staticData->serializedATN);

  const size_t count = staticData->atn->getNumberOfDecisions();
  staticData->decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) { 
    staticData->decisionToDFA.emplace_back(staticData->atn->getDecisionState(i), i);
  }
  statexprlexerLexerStaticData = std::move(staticData);
}

}

StatExprLexer::StatExprLexer(CharStream *input) : Lexer(input) {
  StatExprLexer::initialize();
  _interpreter = new atn::LexerATNSimulator(this, *statexprlexerLexerStaticData->atn, statexprlexerLexerStaticData->decisionToDFA, statexprlexerLexerStaticData->sharedContextCache);
}

StatExprLexer::~StatExprLexer() {
  delete _interpreter;
}

std::string StatExprLexer::getGrammarFileName() const {
  return "StatExpr.g4";
}

const std::vector<std::string>& StatExprLexer::getRuleNames() const {
  return statexprlexerLexerStaticData->ruleNames;
}

const std::vector<std::string>& StatExprLexer::getChannelNames() const {
  return statexprlexerLexerStaticData->channelNames;
}

const std::vector<std::string>& StatExprLexer::getModeNames() const {
  return statexprlexerLexerStaticData->modeNames;
}

const dfa::Vocabulary& StatExprLexer::getVocabulary() const {
  return statexprlexerLexerStaticData->vocabulary;
}

antlr4::atn::SerializedATNView StatExprLexer::getSerializedATN() const {
  return statexprlexerLexerStaticData->serializedATN;
}

const atn::ATN& StatExprLexer::getATN() const {
  return *statexprlexerLexerStaticData->atn;
}




void StatExprLexer::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  statexprlexerLexerInitialize();
#else
  ::antlr4::internal::call_once(statexprlexerLexerOnceFlag, statexprlexerLexerInitialize);
#endif
}
