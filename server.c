/* Istrate Roxana 323CA */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS	10
#define BUFLEN 1024
#define forever 1
#define len 200

typedef struct {
	
	char name[len];
	time_t t_start;
	int port;
} details;
void error(char *msg)
{
    perror(msg);
    exit(1);
}

int find_empty_space(int match[MAX_CLIENTS]){
	int i;
	for (i = 0; i < MAX_CLIENTS; i++){
		if (match[i] == -1){
			return i;
		}
	}
	return -1; // nu mai este loc
}
int main(int argc, char* argv[]){
	
	int sockfd, port_number, fdmax, newsockfd, client_len, match[MAX_CLIENTS];
	int i, j, number_of_bytes, stdinput = 0;;
	struct sockaddr_in server_addr, client_addr;
	details det[MAX_CLIENTS];
	char buffer[BUFLEN],new_buffer[BUFLEN];
	
	fd_set read_fd, tmp_fd;
	
	if (argc < 2){
		printf("Server: Mod de folosire: %s port\n",argv[0]);
		exit(1);
	}
	
	/* Golim multimea de file descriptori de citire (read_fd) si multimea de 
	 * descriptori temporari (tmp_fd) */
	FD_ZERO(&read_fd);
	FD_ZERO(&tmp_fd);
	
	/* Creez socketul pe care va astepta conexiuni din partea altor clienti */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		error("Server: Eroare deschidere socket\n");
	}
	
	port_number = atoi(argv[1]);
	
	/* Asociez socketului un port pe masina locala dupa ce setez campurile structurii */
	memset((char *) &server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port_number);
	if( bind(sockfd,(struct sockaddr *) &server_addr,sizeof(struct sockaddr)) < 0){
		error("Server: Eroare la bind\n");
	}
	listen(sockfd,MAX_CLIENTS);
	/* Adaugam file descriptorul multimii de file descriptori */
	FD_SET(sockfd, &read_fd);
	/* Adaug si file descriptorul pt stdin */
	FD_SET(stdinput, &read_fd);
	fdmax = sockfd; // util pentru functia select
	
	/* Vectorul match imi spune care elemente din vectorul de structuri pentru clienti
	 * chiar contine o informatie despre un client, sau daca este gol */
	for (i = 0; i < MAX_CLIENTS; i++){
		match[i] = -1;
	}
	
	while(forever){
	
		tmp_fd = read_fd;
		if (select(fdmax+1, &tmp_fd, NULL,NULL,NULL) == -1){
			error("Server: Eroare in select\n");
		}
		for (i = 0; i <= fdmax; i++){
			
			if (FD_ISSET(i,&tmp_fd)){
				
				if(i == sockfd){
					/* Daca este file descriptorul pentru asteptare de conexiuni */
					client_len = sizeof(client_addr);
					if((newsockfd = accept(sockfd, (struct sockaddr *) 
						&client_addr, &client_len)) == -1){
							error("Server: Eroare in accept\n");
					}else{
					
						/* Adaug noul socket in multimea de descriptori de citire */
						FD_SET(newsockfd,&read_fd);
						/* Reactualizez fdmax daca este cazul */
						if(newsockfd > fdmax){
							fdmax = newsockfd;
						}
						/* Am o noua conexiune !!! */
						printf("Server: Noua conexiune de la %s, port %d, socket_client %d pe locul %d\n ", 
						inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), newsockfd,j);
						
					}
				}else{
					if (i == 0){
						memset(buffer, 0, BUFLEN);
						fgets(buffer, BUFLEN-1, stdin);
						printf("Server: Am primit de la tastatura comanda %s\n",buffer);
						if (strcmp(buffer, "status\n") == 0){
							printf("Server: Status...\n");
							for (j = 0; j < MAX_CLIENTS; j++){
								if (match[j]!= -1){
									printf("Client %s\tIP 127.0.0.1\tport %d \n",det[j].name,det[j].port);
								}
							}
						}else{
							if (strcmp(buffer, "quit\n") == 0){
								printf("Server: I am leaving...\n");
								for(j = 0; j < MAX_CLIENTS; j++){
									if (match[j] != -1){
										char leave[len] = "i will leave";
										memset(buffer,0,BUFLEN);
										memcpy(buffer,leave,strlen(leave));
										number_of_bytes = send(match[j],buffer,BUFLEN,0);
										if (number_of_bytes < 0){
											error("Server: Nu am putut trimite mesajul de adio\n");
										}else{
											
											FD_CLR(match[j], &read_fd);
											match[j] = -1;
										}
									}
								}
								return 0;
							}else{
								char *p = strtok(buffer," \n");
								if(strcmp(p, "kick") == 0){
									p = strtok(NULL," \n");
									/* Caut clientul prin lista de clienti */
									for (j = 0; j < MAX_CLIENTS; j++){
										if (match[j] != -1 && strcmp(p,det[j].name) == 0){
											char leave[len] = "leave";
											memset(buffer,0,BUFLEN);
											memcpy(buffer,leave,strlen(leave));
											number_of_bytes = send(match[j],buffer,BUFLEN,0);
											if (number_of_bytes < 0){
												error("Server: Nu am putut trimite mesajul de adio\n");
											}else{
 												FD_CLR(match[j], &read_fd);
												printf("Server: I kicked out client %s\n",p);
												match[j] = -1;
												break;
											}
										}
									}
								}
							}
						}
					}else{
						/* Inseamna ca am primit date pe unul dintre socketii cu care pastrez
						 * legatura cu clientii */
						/* Primesc un alt mesaj de la client cu numele lui si portul pe care 
						 * asculta clientul */
						memset(buffer, 0, BUFLEN);
						if ((number_of_bytes = recv(i, buffer, sizeof(buffer),0)) <= 0){
							if (number_of_bytes == 0){
								/* Clientul a inchis conexiunea */
								printf("Server: socket %d hung up\n", i);
							}else{
								error("Server: Eroare la primire\n");
							}
							close(i); 
							FD_CLR(i, &read_fd);
						}else{
							/* Am primit date de la un client pe portul i */
							if (strcmp(buffer,"listclients") == 0){
								
								memset(buffer,0,BUFLEN);
								/* Trebuie sa pun in buffer toate informatiile despre clienti */
								for (j = 0 ; j < MAX_CLIENTS; j++){
									if (match[j] != -1){
										strcat(buffer, det[j].name);
										strcat(buffer,"\n");
									}
								}
								number_of_bytes = send(i, buffer, BUFLEN,0);
								if (number_of_bytes < 0){
									error("Server: Eroare la trimitere mesaj catre client\n");
								}
							}else{
								char verify[len],*p;
								memset(verify,0,len);
								memcpy(verify,buffer,4);
								if(strcmp(verify,"bbye") == 0){
									
									p = strtok(buffer," ");
									p = strtok(NULL," ");
									/* Trebuie sa inchid filedescriptorul i */
									for (j = 0; j < MAX_CLIENTS; j++){
										if (match[j] != -1 && strcmp(det[j].name,p) == 0){
											printf("Server: Clientul %s m-a anuntat ca pleaca\n",det[j].name);
											FD_CLR(match[j], &read_fd);
											match[j] = -1;
											break;
										}
									}
								}else{
								
									char *p = strtok(buffer, " \n");
									if (strcmp(p,"infoclient") == 0){
										p = strtok(NULL," \n");
										printf("Server: Un client vrea informatii despre clientul %s\n",p);
										int ok = 0;
										
										for (j = 0; j < MAX_CLIENTS; j++){
											
											if (match[j] != -1 && strcmp(det[j].name,p) == 0 ){
												int l = strlen(det[j].name);
												memset(new_buffer,0, BUFLEN);
												memcpy(new_buffer,&l,4);
												memcpy(new_buffer+4,det[j].name,l);
												memcpy(new_buffer+4+l,&det[j].port,4);
												memcpy(new_buffer+8+l,&det[j].t_start,sizeof(time_t));
												ok = 1;
												number_of_bytes = send(i,new_buffer,BUFLEN,0);
												if (number_of_bytes < 0){
													error("Server: Eroare la trimitere mesaj catre client\n");
												}
												break;
											}
										}
										if(ok == 0){
											error("Server: Nu am gasit clientul\n");
										}
										
									}else{
										/* In buffer voi pune pe primii 4 bytes portul pe care asculta clientul, 
										* urmatorii 4 bytes lungimea numelui clientului,
										* apoi numele clientului */
										int title_len;
										int port;
										char name[len],reply[len];
										memset(reply,0,len);
										int ok = 0;
										memcpy(&port,buffer,4);
										memcpy(&title_len,buffer+4,4);
										memset(name,0,len);
										memcpy(name,buffer+8,title_len);
										printf("Server: Am primit de la clientul %s cu portul %d\n",name,port);
										/* Trebuie sa verific daca mai este un client cu acelasi nume */
										for (j = 0; j < MAX_CLIENTS; j++){
											if (match[j] != -1 && strcmp(det[j].name, name) == 0){
												/* Am mai primit un client cu acelasi nume */
												ok = 1;
												printf("Server: Am mai primit un client cu acelasi nume\n");
												strcpy(reply,"no");
												number_of_bytes = send(i,reply,len,0);
												break;
											}
										}
										if (ok == 0){
											j = find_empty_space(match);
											if (j == -1){
												error("Server: Nu mai este loc pentru alt client\n");
											}else{
												match[j] = i;	// e setat la newsockfd, folosit pentru send, recv
												strcpy(reply, "welcome ");
												number_of_bytes = send(match[j],reply,len,0);
												strcpy(det[j].name,name);
												det[j].port = port;
												memcpy(&det[j].t_start,buffer+8+title_len,sizeof(time_t));
												printf("Server: I-am setat datele clientului la pozitia %d cu match %d\n",j,i);
												
											} 
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	close(sockfd);	
	return 0;
}
