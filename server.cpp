#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <thread>
#include <experimental/filesystem>
#include <sstream>
#include <iterator>
#include <fstream>
#include <sys/types.h>

using namespace std;

pthread_mutex_t lock; // lock to use during the multi threading
int currentClients = 0; //current number of clients at any given time

void clientHandeling(int clientSocket);
int getTrueLength (string buffer);
string getFileType(string path);
string getDataType(string type);
int sendAll(int s, char *buf, int *len);


int main(int argc, char *argv[])
{
    int serverSocket, clientSocket; //important info to know
    struct sockaddr_in serverAddr;
    struct sockaddr_in clientAddr;
    socklen_t addrSize;

    //intiate mutex lock for multithreading
    if (pthread_mutex_init(&lock, NULL) != 0){ //lock initiation failed
        printf("Lock initiation failed\n"); //error
        exit(-1); //terminate
    }

    int port; //port number
    if (argc != 2){ //no specified port number then use default port
        port = 80;
    }
    else {
        port = atoi(argv[1]); //specifiend port number
    }

    serverSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP); //decalring a socket
    if (serverSocket == -1){ //declaring socket failed
        printf("Failed to create a server socket\n"); //error
        exit(-1);
    }
    memset(&serverAddr, 0, sizeof(serverAddr)); //assiging important info memory, addr, family, port
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1){ // associate the socket with local port on the local machine
        printf("Server socket bind error\n"); 
        exit(-1);
    }

    int listening = listen(serverSocket,50); //listening to any client requests with max 50
    if (listening == -1){
        printf("Server socket listen error\n");
        exit(-1);
    }
    else if (listening == 0){ //waiting for a request
        printf("Listening\n");
    }

    while (1){ //infinite loop
        addrSize = sizeof(clientAddr);
        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addrSize); //accept a client with its socket and address
        if (clientSocket == -1){ //error
            printf("Server socket accept error");
            exit(-1);
        }
        printf("Handling client IP = %s:%d\n", inet_ntoa(clientAddr.sin_addr),clientAddr.sin_port); //start of handeling the client
        pthread_mutex_lock(&lock); // lock the instruction
        currentClients++;
        pthread_mutex_unlock(&lock); //unlock
        thread thr(clientHandeling, clientSocket); //create a new thread
        thr.detach(); //detaching the thread from the object to be executed independently
    }

    return 0;
}

void clientHandeling(int clientSocket){
    bool clientDone = false;
    string fullBuffer = ""; //the full reqeust from client
    char buffer[100]; //current buffer from the client
    while (!clientDone){ //until the client is finished
        struct timeval time;
        pthread_mutex_lock(&lock);
        time.tv_sec = 1800 / currentClients; //default keep alive interval in tcp connections is 30 mins distributed among the current clients
        time.tv_usec = 1800 % currentClients;
        if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*) &time, sizeof(time)) == -1){
            printf("Socket timeout error");
            exit(-1);
        }
        pthread_mutex_unlock(&lock);
        int rcv = recv(clientSocket, buffer, sizeof(buffer),0); //read data transmitted from the client
        if (rcv > 0){ //data exists, no errors
            for (int i = 0; i < 100; i++){ //print the data recieved from the client
                if (buffer[i] == '\0'){
                    break;
                }
                printf("%c", buffer[i]);
            }
            printf("\n");
            string str = string(buffer, rcv); //store the buffer recieved
            fullBuffer += str;
            bool endOfRequest = true;
            int trueLength = getTrueLength (fullBuffer);// actual length of current buffer
            if (trueLength == -1){ //error
                endOfRequest = false;
            }
            while(endOfRequest){ //until the client finishes
                string request = fullBuffer.substr(0, trueLength); //the request needed to be executed
                if(trueLength >= fullBuffer.size()){
                    fullBuffer = ""; //clean the buffer
                }    
                else{
                    fullBuffer = fullBuffer.substr(trueLength);

                }
                string token = request.substr(0, request.find(" ")); //first token 
                string path = "." + request.substr(request.find(" ")+1, request.substr(request.find(" ")+1).find(" ")); //the path or the requested file
                if (token.compare("GET") == 0){ //GET request
                    int fileLength;
                    string response = ""; //response to be sent back to the client
                    try {
                        fileLength = std::experimental::filesystem::file_size(path); //finding the length of the requested file
                    }
                    catch(std::experimental::filesystem::filesystem_error &e){
                        fileLength = -1;
                    }
                    if (fileLength == -1){ //file doesn't exist 
                        response = "HTTP/1.1 404 Not Found\r\n\r\n"; //respond with not found
                    }
                    else { //file exists
                        response = "HTTP/1.1 200 OK\r\n"; //OK response
                        string fileType = getFileType(path); //find the type of the file, HTML, Image or text file
                        string fileName = path.substr(path.find_last_of('/')+1); //find the exact name of the file
                        response += "Content-Type: " + fileType + "\r\nContent-Length: " + to_string(fileLength) + "\r\nContent-Disposition: inline; filename=\"" + fileName + "\"\r\n\r\n";
                        //adding the file type, file length and file name to the response
                        std::ifstream t(path);
                        std::string data ((std::istreambuf_iterator<char>(t)),(std::istreambuf_iterator<char>())); //opening the file and attaching all the data to the response
                        response += data;
                    }
                    int dataLength = response.size();
                    sendAll(clientSocket, &response[0], &dataLength); //sending the response to the client including the data and its size
                }
                else if (token.compare("POST") == 0){ //if the request is a post request
                    string str = request.substr(request.find("Content-Disposition: "),request.find("\r\n", request.find("Content-Disposition: "))-request.find("Content-Disposition: "));
                    istringstream is(str); //find the disposition header, separating it with space then taking the path token to store the filename
                    vector<string> tokens{istream_iterator<string>{is}, istream_iterator<string>{}};
                    size_t first = tokens[2].find_first_of('\"');
                    string filename = "_";
                    filename += tokens[2].substr(first+1,tokens[2].find_last_of('\"')-(first+1));
                    string data = request.substr(request.find("\r\n\r\n")+4); //store the data
                    size_t firstt = request.find("Content-Type: ");
                    string strr = request.substr(firstt, request.find("\r\n", firstt)-firstt);
                    istringstream iss(strr);//find the type header separating with space then storing the type of the data
                    vector<string> tokenss{istream_iterator<string>{iss}, istream_iterator<string>{}};
                    string type = tokenss[1];
                    string dataType = getDataType(type);//get the data type whether it is HTML, image or text
                    size_t starttt = request.find("Content-Length: ");//find the length header, separate, store the data length
                    string strrr = request.substr(starttt, request.find("\r\n", starttt)- starttt);
                    istringstream isss(strrr);
                    vector<string> tokensss{istream_iterator<string>{isss}, istream_iterator<string>{}};
                    int size = stoi(tokensss[1]);
                    std::ofstream file(path+filename, std::ios::binary); //open the specified file
                    file.write(&data[0],size);//write the data recieved
                }
                trueLength = getTrueLength (fullBuffer);
                if (trueLength == -1){
                    endOfRequest = false;
                }
            }
        }
        else {
            clientDone = true; //client is finished
        }
    }
    pthread_mutex_lock(&lock); //decrement the number of current clients
    currentClients --;
    pthread_mutex_unlock(&lock);
    close(clientSocket);
}

int getTrueLength (string buffer){ //get the length of the data
    if (buffer.find("\r\n\r\n") == string::npos){
        return -1;
    }
    else {
        string request = buffer.substr(0, buffer.find("\r\n")); //the main request without headers
        string token = request.substr(0, request.find(" ")); //first token
        if (token.compare("GET") == 0){ //if it is a get request
            return buffer.find("\r\n\r\n")+4; //only the main request length
        }
        else {
            size_t start = buffer.find("Content-Length: "); //if it is POST with headers then send the length specified in the length header
            request = buffer.substr(start, buffer.find("\r\n",start)-start);
            token = request.substr(request.find(" ")+1,request.substr(request.find(" ")+1).find(request.find(" ")));
            int bytes = stoi(token);
            if (buffer.find("\r\n\r\n") + 4 + bytes <= buffer.size()){
                return buffer.find("\r\n\r\n") + 4 + bytes;
            }
            else {
                return -1;
            }
        }
    }
}

string getFileType(string path){ //find type of the file
    string extension = path.substr(path.find_last_of('.')+1);//find the extension found after the last dot
    if (extension.compare("html") == 0) {
        return "text/html";
    }
    else if (extension.compare("jpg") == 0) {
        return "image/jpg";
    }
    else if (extension.compare("jpeg") == 0) {
        return "image/jpeg";
    }
    else if (extension.compare("png") == 0) {
        return "image/png";
    }
    else if (extension.compare("gif") == 0) {
        return "image/gif";
    }
    else if (extension.compare("raw") == 0) {
        return "image/raw";
    }
    else {
        return "text/plain";
    }
}

string getDataType(string type){ //the corrosponding file extension to the file type
    if(type.compare("text/html")== 0){
        return ".html";
    }
    else if(type.compare("image/jpg") == 0){
        return ".jpg";
    }
    else if(type.compare("image/jpeg") == 0){
        return ".jpeg";
    }
    else if(type.compare("image/png") == 0){
        return ".png";
    }
    else if(type.compare("image/gif") == 0){
        return ".gif";
    }
    else if(type.compare("image/raw") == 0){
        return ".raw";
    }
    else{
        return ".txt";
    }
}

int sendAll(int s, char *buf, int *len){//send all the data back to the client
    int total = 0; //bytes sent
    int bytesleft = *len; //bytes left
    int n;
    while (total < *len){
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1){
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *len = total;
    return n==-1?-1:0;
}