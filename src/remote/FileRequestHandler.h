#ifndef FILEREQUESTHANDLER_H
#define FILEREQUESTHANDLER_H
#include "AuthenticatedRequestHandler.h"

class FileRequestHandler : public AuthenticatedRequestHandler {
 public:
  FileRequestHandler(const char* prefix, const char* root);
  virtual void run() override;

 private:
  const char* m_prefix;
  const char* m_root;
};

#endif  // FILEREQUESTHANDLER_H
