#define SUCCESS_CODE 0
#define ERROR_CODE -1
#define DEBUG_PORT 8080
#define BUFFER_SIZE 1024
#define NULL_FD -777

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

using namespace std;

struct FileTransfer {
  string key;
  int uploaderFD;
  int downloaderFD;
};

/* Helpers */
void initiateServer();
void retrieveServerDetails();
void writePortToFile();
void resetSocketFDSet();
void acceptNewClient();
void closeUnmatchedClients();

/* Globals */
// Socket stuff
string serverHostname;
int serverAcceptPort;
struct sockaddr_in serverAddr;
socklen_t serverAddrLen;
int serverAcceptFD;
fd_set socketFDSet;
// App stuff
bool terminateFlag;
int largestFD;
unordered_map<string, struct FileTransfer> fileTransferMap;


/**
 * Main Function
 */
int main(int argc, char *argv[]) {
  try {
    #if DEBUG
    cout << "Server is running." << endl;
    #endif

    initiateServer();
    retrieveServerDetails();
    writePortToFile();

    terminateFlag = false;
    while (true) {
      resetSocketFDSet();

      // Check for activity with select
      // BLOCKS if no activity
      int socketFDActivity;
      if ((socketFDActivity = select(largestFD + 1, &socketFDSet, NULL, NULL, NULL)) < 0) {
        throw "An error occurred while waiting for network activity.";
      }

      // Process clients with activity
      vector<char> buffer;
      for (unordered_map<string, struct FileTransfer>::iterator it = fileTransferMap.begin(); it != fileTransferMap.end(); ++it) {
        struct FileTransfer fileTransfer = it->second;
        // Check uploader (and paired downloader)
        if (fileTransfer.uploaderFD != NULL_FD && fileTransfer.downloaderFD != NULL_FD
            && FD_ISSET(fileTransfer.uploaderFD, &socketFDSet)) {
          int recvSize;
          buffer.clear();
          buffer.assign(BUFFER_SIZE, 0x0);
          // Receive data
          if ((recvSize = recv(fileTransfer.uploaderFD, buffer.data(), BUFFER_SIZE, 0)) < 0) {
            throw "An error occurred while receiving from an uploader.";
          }

          // Client closed connection, upload done
          if (recvSize == 0) {
            string key = fileTransfer.key;
            #if DEBUG
            cout << "Done uploading for FileTransfer with key: " << fileTransfer.key << endl;
            #endif

            // Cleanup uploader
            close(fileTransfer.uploaderFD);
            // Cleanup downloader
            close(fileTransfer.downloaderFD);
            // Cleanup FileTransfer
            fileTransferMap.erase(key);
          }
          // Forward data to downloader
          else {
            if (send(fileTransfer.downloaderFD, buffer.data(), recvSize, 0) < 0) {
              throw "An error occurred while forwarding data to a downloader.";
            }
          }
        }
        // Check downloader
        if (fileTransfer.downloaderFD != NULL_FD
            && FD_ISSET(fileTransfer.downloaderFD, &socketFDSet)) {
          #if DEBUG
          cout << "Downloader has activity for some reason..." << endl;
          #endif
        }
      }

      // Accept new client if any are waiting (and not terminating)
      if (FD_ISSET(serverAcceptFD, &socketFDSet) && !terminateFlag) {
        acceptNewClient();
      }

      // EXIT CONDITION: terminate flagged and no more clients
      if (terminateFlag && fileTransferMap.empty()) {
        #if DEBUG
        cout << "Server will now terminate." << endl;
        #endif
        break;
      }
    }
  }
  catch(const char *e) {
    cerr << e << endl;
    return ERROR_CODE;
  }
  catch(...) {
    cerr << "Unexpected error occurred." << endl;
    return ERROR_CODE;
  }
}


/**
 * Helpers
 */
void initiateServer() {
  // Configure server address
  memset(&serverAddr, '0', sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  #if DEBUG
  serverAddr.sin_port = htons(DEBUG_PORT);
  #else
  serverAddr.sin_port = 0; // Any available port
  #endif
  serverAddrLen = sizeof(serverAddr);
  // Create accepting socket
  if ((serverAcceptFD = socket(AF_INET, SOCK_STREAM, 0)) == 0) {	
    throw "Failed to create accepting socket.";
  }
  // Bind to accepting socket
  if (bind(serverAcceptFD, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
    throw "Failed to bind to accepting socket.";
  }
  // Listen for new connections
  if (listen(serverAcceptFD, SOMAXCONN) < 0) {
    throw "Failed to listen on accepting socket.";
  }
}


void retrieveServerDetails() {
  // Retrieve server details
  // Hostname
  char hostBuff[256];
  gethostname(hostBuff, sizeof(hostBuff));
  serverHostname = hostBuff;
  // Port
  if (getsockname(serverAcceptFD, (struct sockaddr *) &serverAddr, (socklen_t *) &serverAddrLen) < 0) {
    throw "Failed to retrieve server's port.";
  } else {
    serverAcceptPort = stoi(to_string(ntohs(serverAddr.sin_port)));
  }
  #if DEBUG
  cout << "Running on" << endl;
  cout << "\tHostname: " << serverHostname << endl;
  cout << "\tPort: " << serverAcceptPort << endl;
  #endif
}


void writePortToFile() {
  ofstream portFile;
  portFile.open("port", ios::out | ios::trunc);
  portFile << serverAcceptPort << endl;
  portFile.close();
}


void resetSocketFDSet() {
  // Clear the set
  FD_ZERO(&socketFDSet);
  // Add server accepting socket (if not terminating)
  if (!terminateFlag) {
    FD_SET(serverAcceptFD, &socketFDSet);
  }
  largestFD = serverAcceptFD;
  // Add client FDs
  for (unordered_map<string, struct FileTransfer>::iterator it = fileTransferMap.begin(); it != fileTransferMap.end(); ++it) {
    struct FileTransfer ft = it->second;
    if (ft.uploaderFD != NULL_FD) FD_SET(ft.uploaderFD, &socketFDSet);
    if (ft.downloaderFD != NULL_FD) FD_SET(ft.downloaderFD, &socketFDSet);
    int relLargestFD = max(ft.uploaderFD, ft.downloaderFD);
    if (relLargestFD > largestFD) {
      largestFD = relLargestFD;
    }
  }
}


void acceptNewClient() {
  int newClientFD;
  char newClientType;
  string key;

  // Get the new client
  if ((newClientFD = accept(serverAcceptFD, (struct sockaddr *) &serverAddr, (socklen_t *) &serverAddrLen)) < 0) {
    throw "Failed to accept new client.";
  }
  
  // Get command
  char buffer[9];
  if (recv(newClientFD, &buffer, sizeof(buffer), 0) < 0) {
    throw "Failed to get new client's command.";
  }
  if (!(buffer[0] == 'G' || buffer[0] == 'P' || buffer[0] == 'F')) {
    cout << "Received an invalid command from the new client. Rejecting." << endl;
    close(newClientFD);
    return;
  }

  // Set client data
  newClientType = buffer[0];
  key = string(buffer+1, 8);

  // Termination
  if (newClientType == 'F') {
    #if DEBUG
    cout << "Received termination command!" << endl;
    #endif
    terminateFlag = true;
    close(serverAcceptFD);
    closeUnmatchedClients();
    return;
  }
  // Downloader
  else if (newClientType == 'G') {
    struct FileTransfer nft;
    nft.key = key;
    nft.uploaderFD = NULL_FD;
    nft.downloaderFD = newClientFD;
    
    fileTransferMap[nft.key] = nft;
    #if DEBUG
    cout << "New downloader. Adding new FileTransfer with key: " << key << endl;
    #endif
  }
  // Uploader
  else if (newClientType == 'P') {
    struct FileTransfer *ft;
    try {
      ft = &(fileTransferMap.at(key));
    } catch (out_of_range e) {
      throw "New uploader requested for non-existant file transfer.";
    }

    ft->uploaderFD = newClientFD;
    #if DEBUG
    cout << "New uploader matched with FileTransfer with key: " << key << endl;
    #endif
  }

  #if DEBUG
  cout << "Adding new client:" << endl;
  cout << "\tFD: " << newClientFD << endl;
  cout << "\tType: " << newClientType << endl;
  cout << "\tKey: " << key << endl;
  #endif
}


void closeUnmatchedClients() {
  vector<string> ftToDelete;
  for (unordered_map<string, struct FileTransfer>::iterator it = fileTransferMap.begin(); it != fileTransferMap.end(); ++it) {
    struct FileTransfer ft = it->second;
    if (ft.uploaderFD == NULL_FD || ft.downloaderFD == NULL_FD) {
      if (ft.uploaderFD != NULL_FD) close(ft.uploaderFD);
      if (ft.downloaderFD != NULL_FD) close(ft.downloaderFD);
      ftToDelete.push_back(ft.key);
    }
  }
  for (vector<string>::iterator it = ftToDelete.begin(); it != ftToDelete.end(); ++it) {
    fileTransferMap.erase(*it);
    #if DEBUG
    cout << "Cleaning up unmatched FileTransfer: " << *it << endl;
    #endif
  }
}
