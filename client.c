/* Istrate Roxana 323CA */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>

#define MAX_CLIENTS	10
#define BUFLEN 1024
#define forever 1
#define len 200
#define FILE_BUFFER 616
/* 1024 - 2*sizeof(int) - 2 *len = FILE_BUFFER */

typedef struct {
	
	char name[len];
	
	int port;
} details;
typedef struct {
	
	int first, to_receive;
	char sender[len];
	char file2send[len];
	char buffer[BUFLEN];
	
} files;
void error(char *msg)
{
    perror(msg);
    exit(0);
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
int main(int argc, char *argv[]){
	
	int sockfd, number_of_bytes, port, stdinput = 0, i, fdmax, j,file_size,fd2;
	struct sockaddr_in server_addr,client_addr,my_addr;
	struct hostent *server;
	details det[MAX_CLIENTS];
	char history[BUFLEN], *p;
	memset(history,0,BUFLEN);
	int match[MAX_CLIENTS],client_len,newsockfd, ascult, ok, fd, sel, number_received = 0;
	double diff;
	struct stat f_status;
	files f,now;
	struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 500000;
	
	char buffer[BUFLEN], new_buffer[BUFLEN];
	char name[len], receiver[len], file2receive[len];
	time_t t_start,his_start,t_now;
	time(&t_start);
	
	if (argc < 5){
		printf("Client: Mod de folosire: %s <nume_client> <port_client> <ip_server> <port_server> ",argv[0]);
		exit(0);
	}
	
	sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(sockfd < 0){
		error("Client: Eroare deschidere socket\n");
	}
	
		
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(atoi(argv[2]));
	inet_aton(argv[3],&my_addr.sin_addr);
	
	ascult = socket(AF_INET,SOCK_STREAM,0);
	if(ascult < 0){
		error("Client: Eroare deschidere socket\n");
	}
	if(bind(ascult,(struct sockaddr *) &my_addr,sizeof(struct sockaddr)) < 0){
		error("Client: Eroare la bind\n");
	}
	listen(ascult, MAX_CLIENTS);
	
	
	fd_set read_fd, tmp_fd;
	FD_ZERO(&read_fd);
	FD_SET(stdinput,&read_fd);
	FD_SET(sockfd,&read_fd);
	FD_SET(ascult,&read_fd);
	if (ascult > sockfd )
		fdmax = ascult;
	else
		fdmax = sockfd;
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[4]));
	inet_aton(argv[3],&server_addr.sin_addr);
	
	if (connect(sockfd,(struct sockaddr *) &server_addr,sizeof(struct sockaddr)) < 0){
		error("Client: Eroare la conectare\n");
	}
	memset(buffer, 0, BUFLEN);
	port = atoi(argv[2]);
	strcpy(name,argv[1]);
	int l = strlen(name);
	memcpy(buffer,&port,4);
	memcpy(buffer+4,&l,4);
	memcpy(buffer+8,name,l);
	memcpy(buffer+8+l,&t_start,sizeof(time_t));
	number_of_bytes = send(sockfd,buffer,BUFLEN,0);
	if (number_of_bytes < 0){
		error("Client: Eroare la scriere pe socket\n");
	}
	/* Vectorul match imi spune care elemente din vectorul de structuri pentru clienti
	 * chiar contine o informatie despre un client, sau daca este gol */
	for (i = 0; i < MAX_CLIENTS; i++){
		match[i] = -1;
	}
	
	while(forever){	
		
		tmp_fd = read_fd;
		
		if ((sel = select(fdmax+1, &tmp_fd, NULL,NULL,&tv)) == -1){
			error("Client: Eroare in select\n");
		}else{
			if (sel == 0){
				/* Continui trimiterea mesajului */
				/* OBSERVATIE! Sigur am deschisa conexiunea cu el (primul pachet este trimis deja)*/
				for (j = 0; j < MAX_CLIENTS; j++){
					if (match[j] != -1 && strcmp(det[j].name,receiver) == 0){
						
						/* Sa nu mai fie considerata ca o prima trimitere */
						f.first = 0;
						memset(f.buffer,0,BUFLEN);
						number_of_bytes = read(fd,f.buffer,FILE_BUFFER);
						memset(buffer,0,BUFLEN);
						memcpy(buffer,&f,BUFLEN);
						if(number_of_bytes > 0){
							
							number_of_bytes = send(match[j],buffer,BUFLEN,0);
							if (number_of_bytes < 0){
								error("Client: Eroare in trimitere mesaj catre client\n");
							}
						}else{
							printf("Client: S-a terminat de trimis fisierul\n");
							close(fd);
						}
						break;
					}
				}
			}
		}
		for(i = 0; i <= fdmax; i++){
			if(FD_ISSET(i,&tmp_fd)){
				if(i == 0){
					memset(buffer, 0, BUFLEN);
					fgets(buffer, BUFLEN-1, stdin);
					printf("Client: Am primit de la tastatura comanda %s",buffer);	
					if (strcmp(buffer,"listclients\n") == 0){
						
						printf("Client: I want something from server... :)\n");
						char cerere[len] = "listclients";
						memset(buffer,0,BUFLEN);
						memcpy(buffer,cerere,strlen(cerere));
						number_of_bytes = send(sockfd,buffer,BUFLEN,0);
						if (number_of_bytes < 0){
							error("Client: Eroare trimitere mesaj\n");
						}else{
							memset(buffer,0,BUFLEN);
							number_of_bytes = recv(sockfd,buffer,BUFLEN,0);
							if(number_of_bytes < 0){
								error("Client: Eroare la primire mesaj\n");
							}else{
								/* Afisez tot ce mi-a trimis serverul despre clienti */
								printf("Client: Am primit de la server: \n");
								printf("%s",buffer);
							}
						}
					}else{
						memset(new_buffer,0,BUFLEN);
						char *p = strtok(buffer," ");
						if (strcmp(p,"infoclient") == 0){
							printf("Client: I want a piece of info from server...\n");
							p = strtok(NULL," ");
							strcat(new_buffer, "infoclient ");
							strcat(new_buffer, p);
							number_of_bytes = send(sockfd, new_buffer, BUFLEN,0);
							if (number_of_bytes < 0){
								error("Client: Eroare trimitere mesaj\n");
							}else{
								memset(buffer, 0, BUFLEN);
								number_of_bytes = recv(sockfd,buffer,BUFLEN,0);
								if (number_of_bytes < 0){
									error("Client: Eroare la primire mesaj\n");
								}else{
									/* Afisez informatia pe care am primit-o de la server despre 
									 * clientul X*/
									int l;
									memcpy(&l,buffer,4);	// sizeof(int)
									char nume[len];
									memset(nume,0,len);
									memcpy(nume,buffer+4,l);
									int port;
									memcpy(&port,buffer+4+l,4);
									memcpy(&his_start,buffer+8+l,sizeof(time_t));
									time(&t_now);
									printf("Nume client: %s Port: %d Timp: %.0lf\n",nume,
									port,difftime(t_now,his_start));
									j = find_empty_space(match);
									if (j < 0){
										/* Teoretic nu o sa se ajunga niciodata aici */
										error("Client: Nu mai este loc in vectorul de clienti\n");
									}
								}
							}
						}else{
							if(strcmp(p,"message") == 0){
								p = strtok(NULL, " ");
								char nume_client[len];
								memset(nume_client,0,len);
								strcpy(nume_client,p);
								char msg[BUFLEN];
								memset(msg,0,BUFLEN);
								/* Ii pun si numele meu */
								strcpy(msg,argv[1]);
								strcat(msg,": ");
								while ((p = strtok(NULL," \n")) != NULL){
									strcat(msg,p);
									strcat(msg," ");
								}
								strcat(msg,"\n");
								printf("Nume %s mesaj %s",nume_client,msg);
								/* Simulez un infoclient.. => obtin portul */
								memset(new_buffer,0,BUFLEN);
								strcat(new_buffer, "infoclient ");
								strcat(new_buffer,nume_client);
								number_of_bytes = send(sockfd, new_buffer, BUFLEN,0);
								if (number_of_bytes < 0){
									error("Client: Eroare trimitere mesaj\n");
								}else{
									memset(buffer, 0, BUFLEN);
									number_of_bytes = recv(sockfd,buffer,BUFLEN,0);
									if (number_of_bytes < 0){
										error("Client: Eroare la primire mesaj\n");
									}else{
										/* Informatia de la server */
										int l;
										memcpy(&l,buffer,4);
										char nume[len];
										memcpy(nume,buffer+4,l);
										int port;
										memcpy(&port,buffer+4+l,4);
										/* Am aflat numele clientului, caut in vectorul de 
										 * informatii despre client, daca nu cumva deja stiu socketul
										 * pe care sa trimit mesajul */
										 ok = 0;
										for(j = 0; j < MAX_CLIENTS; j++){
											if (match[j] != -1 && strcmp(det[j].name,nume) == 0){
												/* Nu trebuie sa deschid o noua conexiune, 
												 * o am pe cea veche */
												number_of_bytes = send(match[j],msg,BUFLEN,0);
												printf("Client: am trimis pe portul %d, mesajul %s",port,msg);
												if(number_of_bytes < 0){
													/* Clientul a parasit sesiunea */
													printf("Client: Eroare in trimitere mesaj catre client\n");
													match[j] = -1;
												}
												ok = 1;
												break;
											}
										}
										if (ok == 0){
											/* Nu aveam informatii despre client */
											client_addr.sin_family = AF_INET;
											client_addr.sin_port = htons(port);
											inet_aton(argv[3],&client_addr.sin_addr);
										
											newsockfd = socket(AF_INET,SOCK_STREAM,0);
											if(newsockfd < 0){
												error("Client: Eroare deschidere socket\n");
											}
											FD_SET(newsockfd,&read_fd);
											if(newsockfd > fdmax){
												fdmax = newsockfd;
											}
											if (connect(newsockfd,(struct sockaddr *) &client_addr,sizeof(struct sockaddr)) < 0){
												perror("ERROR");
												error("Client: Eroare la conectare\n");
											}
											j = find_empty_space(match);
											if (j < 0){
												error("Client: Nu mai este loc in vectorul de clienti\n");
											}
											/* Am actualizat vectorul de detalii ale clientilor */
											match[j] = newsockfd;
											det[j].port = port;
											strcpy(det[j].name,nume);
											number_of_bytes = send(newsockfd,msg,BUFLEN,0);
											printf("Client: Am trimis lui %s pe portul %d, mesajul %s",nume,port,msg);
											if(number_of_bytes < 0){
												/* Clientul a parasit sesiunea */
												printf("Client: Eroare in trimitere mesaj catre client\n");
												match[j] = -1;
											}
										}
									}
								}
							}else{
								if(strcmp(p,"broadcast") == 0){
									/* Trebuie sa trimit o comanda listclients serverului,
									 * sa retin intr-un buffer numele fiecarui client apoi
									 * sa trimit infoclient cu fiecare client serverului
									 * pentru a afla portul fiecaruia si in cele din urma 
									 * trimit mesajul fiecarui client in parte */
									char msg[BUFLEN];
									memset(msg,0,BUFLEN);
									strcpy(msg,argv[1]);
									strcat(msg,": ");
									while ((p = strtok(NULL," \n")) != NULL){
										strcat(msg,p);
										strcat(msg," ");
									}
									strcat(msg,"\n");
									char cerere[len] = "listclients";
									memset(buffer,0,BUFLEN);
									memcpy(buffer,cerere,strlen(cerere));
									number_of_bytes = send(sockfd,buffer,BUFLEN,0);
									if (number_of_bytes < 0){
										error("Client: Eroare trimitere mesaj\n");
									}else{
										memset(buffer,0,BUFLEN);
										number_of_bytes = recv(sockfd,buffer,BUFLEN,0);
										if(number_of_bytes < 0){
											error("Client: Eroare la primire mesaj\n");
										}else{
											/* In buffer sunt numele clientilor existenti */
											char *q = strtok(buffer,"\n");
											j = 0;
											while(q != NULL){
												
												ok = 0;
												for (j = 0; j < MAX_CLIENTS; j++){
													if (match[j] != -1 && strcmp(det[j].name,q) == 0){
														/* Aveam informatii despre acel client 
														 * pur si simplu ii trimit mesajul */
														number_of_bytes = send(match[j],msg,BUFLEN,0);
														printf("Client: am trimis pe portul %d, mesajul %s\n",port,msg);
														ok = 1;
														if(number_of_bytes < 0){
															/* Clientul a parasit sesiunea */
															printf("Client: Eroare in trimitere mesaj catre client\n");
															match[j] = -1;
														}
													}
												}
												if (ok == 0){
													/* Nu aveam informatii despre client */
													/* Interoghez serverul pentru a afla portul */
													memset(new_buffer,0,BUFLEN);
													strcat(new_buffer, "infoclient ");
													strcat(new_buffer,q);
													number_of_bytes = send(sockfd, new_buffer, BUFLEN,0);
													if (number_of_bytes < 0){
														error("Client: Eroare trimitere mesaj\n");
													}else{
														memset(new_buffer, 0, BUFLEN);
														number_of_bytes = recv(sockfd,new_buffer,BUFLEN,0);
														if (number_of_bytes < 0){
															error("Client: Eroare la primire mesaj\n");
														}else{
															int l;
															memcpy(&l,new_buffer,4);
															char nume[len];
															memset(nume,0,len);
															memcpy(nume,new_buffer+4,l);
															int port;
															memcpy(&port,new_buffer+4+l,4);
															/* Deschid o noua conexiune cu el */
															client_addr.sin_family = AF_INET;
															client_addr.sin_port = htons(port);
															inet_aton(argv[3],&client_addr.sin_addr);
														
															newsockfd = socket(AF_INET,SOCK_STREAM,0);
															if(newsockfd < 0){
																error("Client: Eroare deschidere socket\n");
															}
															FD_SET(newsockfd,&read_fd);
															if(newsockfd > fdmax){
																fdmax = newsockfd;
															}
															if (connect(newsockfd,(struct sockaddr *) &client_addr,sizeof(struct sockaddr)) < 0){
																perror("ERROR");
																error("Client: Eroare la conectare\n");
															}
															j = find_empty_space(match);
															if (j < 0){
																error("Client: Nu mai este loc in vectorul de clienti\n");
															}
															/* Am actualizat vectorul de detalii ale clientilor */
															match[j] = newsockfd;
															det[j].port = port;
															strcpy(det[j].name,nume);
															number_of_bytes = send(newsockfd,msg,BUFLEN,0);
															printf("Client: Am trimis lui %s pe portul %d, mesajul %s\n",nume,port,msg);
															if(number_of_bytes < 0){
																/* Clientul a parasit sesiunea */
																printf("Client: Eroare in trimitere mesaj catre client\n");
																match[j] = -1;
															}
														}
													}
												}
												q = strtok(NULL,"\n");
											}
										}
									}
								}else{
									if (strcmp(p,"sendfile") == 0){
										/* Numele clientului */
										p = strtok(NULL," \n");
										memset(receiver, 0, len);
										strcpy(receiver, p);
										/* Numele fisierului */
										p = strtok(NULL," \n");
										memset(f.file2send,0,len);
										strcpy(f.file2send,p);
										fd = open(f.file2send,O_RDONLY);
										fstat(fd,&f_status);
										file_size = (int) f_status.st_size;
										f.to_receive = file_size;
										f.first = 1;
										memset(f.sender,0,len);
										strcpy(f.sender,argv[1]);
										memset(f.buffer,0,BUFLEN);
										/* Verific daca nu cumva cunosc socketul destinatarului 
										 * si am deja o conexiune cu el */
										ok = 0;
										for (j = 0; j < MAX_CLIENTS; j++){
											if (match[j] != -1 && strcmp(det[j].name,receiver) == 0){
												
												number_of_bytes = read(fd,f.buffer,FILE_BUFFER);
												memset(buffer,0,BUFLEN);
												memcpy(buffer,&f,BUFLEN);
												if(number_of_bytes > 0){
							
													number_of_bytes = send(match[j],buffer,BUFLEN,0);
													if (number_of_bytes < 0){
														error("Client: Eroare in trimitere mesaj catre client\n");
													}
												}else{
													printf("Client: S-a terminat de trimis fisierul\n");
													close(fd);
												}
												ok = 1;
												break;
											}
										}
										if (ok == 0){
										
											/* Nu aveam deschisa conexiunea cu clientul X */
											/* Deschid o noua conexiune cu el */
											/* Simulez un infoclient.. => obtin portul */
											int port;
											memset(new_buffer,0,BUFLEN);
											strcat(new_buffer, "infoclient ");
											strcat(new_buffer,receiver);
											number_of_bytes = send(sockfd, new_buffer, BUFLEN,0);
											if (number_of_bytes < 0){
												error("Client: Eroare trimitere mesaj\n");
											}else{
												memset(buffer, 0, BUFLEN);
												number_of_bytes = recv(sockfd,buffer,BUFLEN,0);
												if (number_of_bytes < 0){
													error("Client: Eroare la primire mesaj\n");
												}else{
													/* Informatia de la server */
													int l;
													memcpy(&l,buffer,4);
													char nume[len];
													memcpy(nume,buffer+4,l);
													memcpy(&port,buffer+4+l,4);
												}
											}	
											client_addr.sin_family = AF_INET;
											client_addr.sin_port = htons(port);
											inet_aton(argv[3],&client_addr.sin_addr);
										
											newsockfd = socket(AF_INET,SOCK_STREAM,0);
											if(newsockfd < 0){
												error("Client: Eroare deschidere socket\n");
											}
											FD_SET(newsockfd,&read_fd);
											if(newsockfd > fdmax){
												fdmax = newsockfd;
											}
											if (connect(newsockfd,(struct sockaddr *) &client_addr,sizeof(struct sockaddr)) < 0){
												perror("ERROR");
												error("Client: Eroare la conectare\n");
											}
											j = find_empty_space(match);
											if (j < 0){
												error("Client: Nu mai este loc in vectorul de clienti\n");
											}
											/* Am actualizat vectorul de detalii ale clientilor */
											match[j] = newsockfd;
											det[j].port = port;
											strcpy(det[j].name,receiver);
											/* Trimit primul pachet */
											number_of_bytes = read(fd,f.buffer,FILE_BUFFER);
											if(number_of_bytes > 0){
												
												memset(buffer,0,BUFLEN);
												memcpy(buffer,&f,BUFLEN);
												number_of_bytes = send(match[j],buffer,BUFLEN,0);
												if (number_of_bytes < 0){
													error("Client: Eroare in trimitere mesaj catre client\n");
												}
											}else{
												printf("Client: S-a terminat de trimis fisierul\n");
												close(fd);
												break;
											}
										}
									}else{
										if(strcmp(p,"history\n") == 0){
											/* Trebuie afisata structura history */
											printf("%s",history);
										}else{
											if(strcmp(p,"quit\n") == 0){
												/* Anunt serverul si ceilalti clienti ca ma retrag */
												/* Creez mesajul de goobye si il trimit tuturor */
												char leave[len] = "bbye ";
												memset(buffer,0,BUFLEN);
												strcpy(buffer, leave);
												strcat(buffer,argv[1]);
												for (j = 0; j < MAX_CLIENTS; j++){
													if (match[j] != -1){
														number_of_bytes = send(match[j],buffer,BUFLEN,0);
														if (number_of_bytes < 0){
															error("Server: Nu am putut trimite mesajul de adio\n");
														}
													}
												}
												/* Inclusiv serverului */
												number_of_bytes = send(sockfd,buffer,BUFLEN,0);
												if (number_of_bytes < 0){
													error("Server: Nu am putut trimite mesajul de adio\n");
												}
												return 0;
											}
											
										}	
									}
								}
							}				
						}
					}
				}else{
					if (i == sockfd){
						memset(buffer, 0, BUFLEN);
						if ((number_of_bytes = recv(i, buffer, BUFLEN,0)) > 0){
							printf("Client: Am primit de la server comanda %s\n",buffer);
							if(strcmp(buffer,"leave") == 0 || strcmp(buffer, "i will leave") == 0 || strcmp(buffer, "no") == 0){
								close(sockfd);
								return 0;
							}
							
						}
						
					}else{
						
						if(i == ascult){
						
							printf("Client: Am o cerere de conexiune :D\n");
							client_len = sizeof(client_addr);
							if((newsockfd = accept(ascult, (struct sockaddr *) 
								&client_addr, &client_len)) == -1){
									error("Server: Eroare in accept\n");
							}else{
							
								/* Adaug noul socket in multimea de descriptori de citire */
								FD_SET(newsockfd,&read_fd);
								/* Reactualizez fdmax daca este cazul */
								if(newsockfd > fdmax){
									fdmax = newsockfd;
								}
							}
							
						}else{
							
							/* Inseamna ca am primit un mesaj de la alt client */
							printf("Client: Am primit un mesaj!!\n");
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
								char verify[len];
								memset(verify,0,len);
								memcpy(verify,buffer,4);
								if(strcmp(verify,"bbye") == 0){
									/* Unul dintre clienti se retrage */
									p = strtok(buffer," ");
									p = strtok(NULL," ");
									for (j = 0; j < MAX_CLIENTS; j++){
										if(match[j] != -1 && strcmp(det[j].name,p) == 0){
											FD_CLR(match[j], &read_fd);
											match[j] = -1;
											printf("Client: S-a retras clientul %s\n",det[j].name);
											break;
										}
									}
									
								}else{
									/* Daca este un mesaj de tip primire de fisier primul camp
									 * al structurii files este setat fie pe 1 fie pe 0 */
									memset(&now,0,sizeof(files));
									memcpy(&now,buffer,BUFLEN);
									if (now.first == 1){
										/* Este primul send al unui fisier */
										/* Deschidem fisierul, aflam numele senderului,
										 * numele fisierului, contruim fisierul de receive,
										 * primim date */
										
										memset(file2receive,0,len);
										strcpy(file2receive,now.file2send);
										strcat(file2receive,"_primit");
										fd2 = open(file2receive,O_WRONLY | O_TRUNC | O_CREAT, 0666);
										printf("Client: Am primit o parte dintr-un fisier numit %s, de la %s\n",now.file2send,now.sender);
										strcat(history,now.sender);
										strcat(history,": ");
										strcat(history,now.file2send);
										strcat(history,"\n");
										int l = strlen(now.buffer);
										write(fd2,now.buffer,l);
										number_received += l;
										if(number_received == now.to_receive){
											printf("Client: Am incheiat transmiterea fisierului\n");
											close(fd2);
										}
									}else{
										if(now.first == 0){
											
											printf("Client: Am primit inca o parte dintr-un fisier numit %s, de la %s\n",now.file2send,now.sender);
											int l = strlen(now.buffer);
											write(fd2,now.buffer,l);
											number_received += l;
											if(number_received == now.to_receive){
												printf("Client: Am incheiat transmiterea fisierului\n");
												close(fd2);
											}
										}else{
											printf("%s",ctime(&t_start));
											printf("%s",buffer);
											strcat(history,buffer);
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
	return 0;
}
