// Name:        Caleb Priester
// Class:       CPSC3600
// Assignment:  HW2
//
// Description: dnsq.c allows a user to specify a DNS server IP and a name to be
//  interpretted (as well as timeout, retries and port number, but these are 
//  optional).  This builds a simple query byte by byte and sends it to the 
//  specified DNS server, and receives the response sent back.  This response
//  is then interpretted and each answer is printed.

#include "dnsqAssg.h"

int sock;

// main: The main method for dnsq. This holds all logic for creating the DNS
//  query, sending the message to a specified DNS server, and interpretting
//  the response returned by the server.
int main(int argc, char *argv[]) {
   struct sockaddr_in servAddr;
   struct sockaddr_in fromAddr;
   struct hostent *thehost;
   unsigned int fromSize;
   void *buffer = malloc(500);
   fd_set readfds;
   struct timeval tv;

   int timeout = 5;
   int retries = 3;
   unsigned short servPort = 53;
   char *servIP;
   char *name;
   int nameLen;
   unsigned short id;
   void *message;

   unsigned short ansCt;
   char isAuth;
   char print;

   unsigned short type;
   unsigned short class;
   unsigned int ttl;
   unsigned short rdlen;

   //Interpret commands and set variables accordingly
   int i;
   for(i = 1; i < argc && argv[i][0] == '-'; i++) {
      if(argv[i][1] == 't') timeout = atoi(argv[++i]);
      else if(argv[i][1] == 'r') retries = atoi(argv[++i]);
      else if(argv[i][1] == 'p') servPort = atoi(argv[++i]);
   }
   servIP = argv[i++]+1;
   name = argv[i];

   //Finds the length of the passed-in name
   char *temp = name;
   for(nameLen = 0; *temp != '\0'; nameLen++) temp++;
   message = malloc(sizeof(char)*(18 + nameLen));

   //Generate a random id
   srand(time(NULL));
   id = rand();
   
   //Build header
   *((unsigned short *)message) = htons(id);           //ID
   *((unsigned char *)(message+2)) = 0x01;             //QR, OpCode, AA, TC, RD
   *((unsigned char *)(message+3)) = 0x00;             //RA, Z, RCODE
   *((unsigned short *)(message+4)) = htons(0x0001);   //QDCOUNT
   *((unsigned short *)(message+6)) = htons(0x0000);   //ANCOUNT
   *((unsigned short *)(message+8)) = htons(0x0000);   //NSCOUNT
   *((unsigned short *)(message+10)) = htons(0x0000);  //ARCOUNT

   //Append QNAME to body
   unsigned char len = 0;
   int lenLoc = 12;
   int iter = 13;
   temp = name;
   while(*temp != '\0') {
      if(*temp == '.') {
         *((unsigned char *)(message+lenLoc)) = len;
	 len = 0;
	 lenLoc = iter;
      }
      else {
         *((unsigned char *)(message+iter)) = *temp;
	 len++;
      }
      iter++;
      temp++;
   }
   *((unsigned char *)(message+lenLoc)) = len;

   //Build rest of body
   *((unsigned char *)(message+iter)) = 0x00;              //End of QNAME
   *((unsigned short *)(message+iter+1)) = htons(0x0001);  //QTYPE
   *((unsigned short *)(message+iter+3)) = htons(0x0001);  //QCLASS
   iter = iter+5;

   //Code to open socket for connecting to DNS server
   if((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
      printf("Socket failed.\n");
      exit(1);
   }

   fcntl(sock, F_SETFL, O_NONBLOCK);

   //FD_ZERO(&readfds);
   //FD_SET(sock, &readfds);

   memset(&servAddr, 0, sizeof(servAddr));
   servAddr.sin_family = AF_INET;
   servAddr.sin_addr.s_addr = inet_addr(servIP);
   servAddr.sin_port = htons(servPort);

   if(servAddr.sin_addr.s_addr == -1) {
      thehost = gethostbyname(servIP);
      servAddr.sin_addr.s_addr = *((unsigned long *) thehost->h_addr_list[0]);
   }

   //Send DNS query and receive response. Try "retry" + 1 times if response is
   // not received in"timeout" time.
   retries = retries+1;
   while(retries > 0) { 
      tv.tv_sec = timeout;
      tv.tv_usec = 0;

      FD_ZERO(&readfds);
      FD_SET(sock, &readfds);

      sendto(sock, (char *)message, 18+nameLen, 0,
        (struct sockaddr *) &servAddr, sizeof(servAddr));
      fromSize = sizeof(fromAddr);
      
      if(select(sock + 1, &readfds, NULL, NULL, &tv)) {
         recvfrom(sock, buffer, 500, 0, (struct sockaddr *) &fromAddr, 
            &fromSize);   
       	 break;
      }
      retries--;
      //All retries used and send failed.
      if(retries == 0) {
         printf("ERROR\tMessage failed to send. Out of retries.\n");
	 exit(0);
      } else {
         printf("ERROR\tMessage failed to send. Retrying.\n");
      }
   }

   /*--------------Interpretting Response---------------*/

   //AA bit
   isAuth = *((char *)(buffer+2))>>2 & 1;

   //TC bit (error on 1)
   if(((*((char *)(buffer+2))>>1) & 1) == 1) {
      printf("ERROR\tThe message was truncated.\n");
      exit(0);
   }

   //RA bit (error on 0)
   if(((*((char *)(buffer+3))>>7) & 1) == 0) {
      printf("ERROR\tThe server does not support recursion.\n");
      exit(0);
   }

   //RCODE (error when not zero, except NOTFOUND when 3
   char rcode = *((char *)(buffer+3)) & 0xF;
   if(rcode == 1) {
      printf("ERROR\tThe query was improperly formatted.\n");
      exit(0);
   }
   else if(rcode == 2) {
      printf("ERROR\tThe server failed.\n");
      exit(0);
   }
   else if(rcode == 3) {
      printf("NOTFOUND\n");
      exit(0);
   }
   else if(rcode == 4) {
      printf("ERROR\tThe server doesn't support this kind of query.\n");
      exit(0);
   }
   else if(rcode == 5) {
      printf("ERROR\tThe server doesn't like you.\n");
      exit(0);
   }

   //ANSCOUNT used for interpretation
   ansCt = ntohs(*((unsigned short *)(buffer+6)));

   //Interpretting and printing each answer
   while(ansCt > 0) {
      print = 1;  //print indicates whether the results should be printed
                  // print = 1 iff the type is 0x0001 OR 0x0005 
		  // and the class is 0x0001.

      //Skip name section of answer
      while(*((char *)(buffer+iter)) != 0) {
         if((*((char *)(buffer+iter))>>6 & 3) == 3) {
	    iter+=2;
	    break;
	 }
         iter++;
      }

      //TYPE (must be 1 or 5, else error and print = 0)
      type = ntohs(*((unsigned short *)(buffer+iter)));
      
      //CLASS (must be 1, else error and print = 0) 
      iter+=2;
      class = ntohs(*((unsigned short *)(buffer+iter)));
      if(class != 1) {
         printf("ERROR\tUnrecognizable response data class.\n");
	 print = 0; 
      }
      
      //Since TYPE involves printing, process after class is interpretted
      if(type == 1) {if(print) printf("IP\t");}
      else if(type == 5) {if(print) printf("CNAME\t");}
      else {
         if(print) {
            printf("ERROR\tUnrecognizable response data type.\n");
	    print = 0;
	 }
      }

      //TTL
      iter+=2;
      ttl = ntohl(*((unsigned int *)(buffer+iter)));

      //RDLENGTH
      iter+=4;
      rdlen = ntohs(*((unsigned short *)(buffer+iter)));

      //Process RDATA
      //If the type is an IP.
      if(type == 1) {
         iter+=2;
	 for(i = 0; i < 4; i++) {
	    printf("%d", *((unsigned char *)(buffer+iter)));
	    if(i != 3) printf(".");
	    iter++;
	 }
      }

      //If the type is a CNAME.
      if(type == 5) {
         iter+=2;
         i = 0;
         unsigned int tempIter = 0;
         while(i < rdlen) {
            len = *((unsigned char *)(buffer+iter));

	    //This indicates a pointer
	    if((len>>6 & 3) == 3) {
	       //Set the tempIter to be the specified offset.
	       tempIter = ntohs(*((unsigned short *)(buffer+iter))) & 0x3F;
	       iter+=2;
	       break;
	    }
	    iter++;
	    i++;

	    //Logic for printing each segment.
            while(len != 0) {
	       if(print) printf("%c", *((unsigned char *)(buffer+iter)));
	       iter++;
	       i++;
	       len--;
	    }
	    if(i != rdlen) if(print) printf(".");
         }

         //If a pointer was encountered, parse the name pointed to.
         if(tempIter) {
	 len = 0;
            while(*((unsigned char *)(buffer+tempIter)) != 0) {
	       if(!len) {
	          len = *((unsigned char *)(buffer+tempIter));
		  tempIter++;
	       }

	       if(print) printf("%c", *((unsigned char *)(buffer+tempIter)));
	       len--;
	       tempIter++;

               if(len == 0 && *((unsigned char *)(buffer+tempIter)) != 0) 
	          printf(".");
	    }
	 }

      }
      printf("\t%d", ttl);
      if(isAuth) printf("\tauth\n");
      else printf("\tnonauth\n");
      ansCt--;
   }
}
