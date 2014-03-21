#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"
#include <math.h>
int max (int a, int b){
	if ( a > b ){
		return a;
	}
	return b;
}
int min (int a, int b){
	if (a < b)
		return a;
	return b;
}
int first_task(char *filename, int speed, int delay){

	msg t;
	int i,result,fd,len ;
	int BDP, window_size, file_size,number_of_frames,MAX = 50;
	struct stat f_status;
	char *file = (char*)malloc(MAX * sizeof(char));
	strcpy(file,"recv_");
	strcat(file,filename);

	/* Deschidem fisierul */
	fd = open(filename,O_RDONLY);
	fstat(fd,&f_status);
	/* file_size =  numarul in bytes al fisierului */
	file_size = (int) f_status.st_size;

	BDP = speed * delay * 1000;
	printf("[SENDER] Sender starts.\n");
	printf("[SENDER] Filename=%s, task_index=%d, speed=%d, delay=%d\n", filename, 0, speed, delay);
	printf("[SENDER] Waiting for the transfer to finish..\n");

	init(HOST, PORT1);
	number_of_frames = file_size / MSGSIZE;
	if (file_size % MSGSIZE != 0){
		number_of_frames++;
	}

	/* Initial trimit numarul de cadre ce vor fi receptionate */
	memset(&t,0,sizeof(msg));
	t.len = number_of_frames;
	len = strlen(file);
	memcpy(t.payload,&len,4);
	memcpy(t.payload+4,file,strlen(file));
	result = send_message(&t);
	if (result < 0){

		perror("[SENDER] Send error.\n");
		return -1;
	}
	result = recv_message(&t);
	if (result < 0){
		perror("[SENDER] Receive error.\n");
		return -1;
	}

	/* window size =  numarul de cadre fara ack */
	window_size = BDP/(8*(MSGSIZE+4));
	printf("[SENDER] Window_size = %d frames\n", window_size);

	memset(&t,0,sizeof(msg));
	for (i = 0; i < window_size; i++){

		t.len = read(fd,t.payload,MSGSIZE);
		result = send_message(&t);
		if (result < 0){
			perror("[SENDER] Send error.\n");
			return -1;
		}
	}

	/* De acum numaram ACK urile primite
	 * un nou ACK primit reprezinta un nou loc
	 * in legatura de date
	 */
	for (i = 0; i < number_of_frames-window_size; i++){

		/* Asteptam un ACK*/
		result = recv_message(&t);
		if (result < 0){
			perror("[SENDER] Receive error. \n");
			return -1;
		}
		t.len = read(fd,t.payload,MSGSIZE);
		result = send_message(&t);
		if (result < 0){
			perror("[SENDER] Send error. \n");
			return -1;
		}

	}
	/* Inca mai sunt de primit window_size ACKs */
	for ( i = 0; i < window_size; i++){

		result = recv_message(&t);
		if (result < 0){
			perror("[SENDER] Receive error. \n");
			return -1;
		}
	}
	printf("[SENDER] Job done, all sent.\n");
	close(fd);
	free(file);
	return 0;
}
int second_task(char *filename, int speed, int delay){

	msg t;
	int fd, file_size, i, expected_ack = 0,message_size = MSGSIZE-sizeof(int),MAX = 50;
	struct stat f_status;

	char *file = (char*)malloc(MAX * sizeof(char));
	strcpy(file,"recv_");
	strcat(file,filename);

	printf("[SENDER] Sender starts.\n");
	printf("[SENDER] Filename=%s, task_index=%d, speed=%d, delay=%d\n", filename, 1, speed, delay);
	printf("[SENDER] Waiting for the transfer to finish..\n");

	init(HOST,PORT1);
	fd = open(filename,O_RDONLY);
	fstat(fd,&f_status);
	file_size = (int) f_status.st_size;
	int next_frame_to_send = 0, received_ack;
	int BDP, window_size, number_of_frames;

	BDP = speed * delay * 1000;

	/* window size =  numarul de pachete ce se afla pe legatura de date */
	window_size = BDP/(8*(MSGSIZE+4));
	number_of_frames = file_size / (MSGSIZE - 4);
	if (file_size % (MSGSIZE-4) != 0){
		number_of_frames++;
	}
	msg buffer[window_size];

	/* Inital trimit numarul de cadre ce vor fi primte de reciever
	 * si numele fisierului unde acesta va scrie mesajele */
	memset(&t,0,sizeof(msg));
	t.len = strlen(file);
	memcpy(t.payload,&number_of_frames,4);
	memcpy(t.payload+sizeof(int),file,t.len);
	send_message(&t);

	while (recv_message_timeout(&t,max(1000,2*delay)) == -1){
		while( send_message(&t) < 0){ }
	}
	/* Se trimit intial window_size cadre */
	memset(&t,0,sizeof(msg));
	for (i = 0; i < window_size; i++){

		t.len = read(fd,t.payload,message_size);
		memcpy(t.payload+message_size,&next_frame_to_send,4);
		send_message(&t);
		next_frame_to_send ++;
		buffer[i] = t;
	}
	while (1){

		if (recv_message_timeout (&t, max(1000,2*delay)) == -1){
			/* Daca receveirul nu a trimis confirmare pentru pachetul x
			 * senderul trimite o fereastra, incepand de la ultimul pachet receptionat corect + 1 */
			for (i = expected_ack % window_size; i < window_size; i++){
				send_message(&buffer[i]);

			}
			for (i = 0 ; i < expected_ack % window_size; i++){
				send_message(&buffer[i]);
			}
		}else{
			memcpy(&received_ack,t.payload+message_size,4);
			if (received_ack == number_of_frames){
				/* Daca am primit si ACK pentru ultimul pachet se incheie algoritmul */
				break;
			}else{
				/* Daca nu am trimis pachetul de final si urmatorul pachet care va fi trimis
				 * nu este este cel de incheiere a legaturii, il trimit, iar daca
				 * e receptionat corect se incheie transferul */

				t.len= read (fd, t.payload, message_size);
				memcpy (t.payload+message_size, &next_frame_to_send, 4);
				next_frame_to_send ++;
				buffer [received_ack%window_size] = t;
				send_message (&t);
				expected_ack ++;
			}

		}

	}
	printf("[SENDER] Transfer finished! :D\n");
	close(fd);
	free(file);
	return 0;
}
int third_task(char *filename, int speed, int delay){

	msg t;
	int fld, file_size, i, expected_ack = 0, received_ack,message_size = MSGSIZE-sizeof(int), found_next_ack, MAX = 50;
	int next_frame2send = 0, BDP, window_size, number_of_frames;
	struct stat f_status;
	char *file = (char*)malloc(MAX * sizeof(char));
	strcpy(file,"recv_");
	strcat(file,filename);

	printf("[SENDER] Sender starts.\n");
	printf("[SENDER] Filename=%s, task_index=%d, speed=%d, delay=%d\n", filename, 2, speed, delay);
	printf("[SENDER] Waiting for the transfer to finish..\n");

	init(HOST,PORT1);

	fld = open(filename,O_RDONLY);
	fstat(fld,&f_status);
	file_size = (int) f_status.st_size;
	BDP = speed * delay * 1000;
	/* window size =  numarul de cadre din legatura de date */
	window_size = BDP/(8*(MSGSIZE + 4));

	number_of_frames = file_size / (MSGSIZE - 4);
	if (file_size % (MSGSIZE - 4) != 0){
		number_of_frames++;
	}
	msg buffer[window_size];
	/* Un match pe buffer care retine pentru care
	 * pachete s-a primit confimare */
	int mirror[window_size];

	/* Initial trimit numarul de cadre pe care le va
	 * receptiona receiverul impreuna cu window_size-ul
	 * si numele fisierului de output */
	memset(&t,0,sizeof(msg));
	t.len = strlen(file);
	memcpy(t.payload,&number_of_frames,4);
	memcpy(t.payload+sizeof(int),&window_size,4);
	memcpy(t.payload+2*sizeof(int),file,t.len);
	//printf("[sender] number of frames %d ");
	send_message(&t);

	while (recv_message_timeout(&t,max(1000,2*delay)) == -1){
		while( send_message(&t) < 0){ }
	}

	memset(&t,0,sizeof(msg));
	/* Senderul trimite o fereastra de cadre pentru
	 * a mentine ocupata legatura de date*/
	for (i = 0; i < window_size; i++){

		t.len = read(fld,t.payload,message_size);
		memcpy(t.payload+message_size,&next_frame2send,4);
		buffer[i] = t;
		mirror[i] = -1;
		send_message(&t);
		next_frame2send++;
	}

	number_of_frames--;

	while (1){
		if (recv_message_timeout (&t, max(1000,2*delay)) == -1){
			/* Daca s-a intrat in time-out Senderul retrimite toate
			 * pachetele pentru care nu a primit confimare */
			for (i = expected_ack % window_size; i < window_size; i++){
				if (mirror[i] == -1){
					send_message (&buffer[i]);
				}
			}
			for (i = 0; i < expected_ack % window_size; i++){
				if (mirror[i] == -1){
					send_message (&buffer[i]);
				}
			}
		}else{
			memcpy (&received_ack, t.payload+message_size, 4);
			/* In cazul in care pachetul pe care il asteptam este cel de final si
			 * s-a primit confimare pentru acesta, se incheie transferul */
			if ( (received_ack == number_of_frames) && (expected_ack == number_of_frames)){
				break;
			}else{

				if (received_ack != expected_ack){
					/* Daca s-a primit un ACK si nu este cel pe care il asteptam
					 * marchez cadrul ca fiind receptionat */
					mirror[received_ack % window_size] = 1;
				}else{
					/* Daca ACK ul primit este cel pe care il asteptam
					 * caut prin bufferul rotit cate pachete pe langa acesta au mai fost
					 * confirmate, pentru a sti cate pachete trebuie trimise */
					t.len = read (fld, t.payload, message_size);
					if (t.len > 0){
						/* Inca au mai fost cadre de citit din fisier
						 * sunt pusi in buffer si marcati ca fiind neconfirmati */
						memcpy (t.payload+message_size, &next_frame2send, 4);
						buffer [received_ack % window_size] = t;
						mirror[received_ack % window_size] = -1;
						send_message (&t);
						next_frame2send ++;
					}else{
						mirror[received_ack % window_size] = 1;
					}
					expected_ack ++;

					found_next_ack = 0;
					for (i = expected_ack % window_size; i < window_size; i++){
						if (mirror[i] == 1 && found_next_ack == 0){
							t.len =  read (fld, t.payload, message_size);
							if (t.len > 0){
								/* Inca au mai fost cadre de citit din fisier
								* sunt pusi in buffer si marcati ca fiind neconfirmati */
								memcpy (t.payload+message_size, &next_frame2send, 4);
								buffer [i] = t;
								mirror[i] = -1;
								send_message (&t);
								next_frame2send ++;
							}
							expected_ack ++;
						}else{
							found_next_ack = 1;
							break;
						}
					}
					/* Trebuie reactualizat urmatorul ACK ce trebuie receptionat */
					if (found_next_ack == 0){
						for (i = 0; i < received_ack % window_size; i++){
							if (mirror[i] == 1 && found_next_ack == 0){
								t.len =   read (fld, t.payload, message_size);
								if (t.len > 0){
									/* Inca au mai fost octeti de citit din fisier
									* ii pun in buffer si il marchez ca fiind neconfirmat */
									memcpy (t.payload+message_size, &next_frame2send, 4);
									buffer [i] = t;
									mirror[i] = -1;
									send_message (&t);
									next_frame2send ++;
								}
								expected_ack ++;
							}else{
								found_next_ack = 1;
								break;
							}
						}
					}
					/* Daca urmatorul ACK depaseste numarul de cadre se incheie transferul */
					if (expected_ack > number_of_frames){
						break;
					}
				}

			}

		}
	}
	printf("[SENDER] Transfer finished! :D\n");
	close(fld);
	free(file);
	return 0;
}
char set_bit(char byte, int number){

	return (byte = byte | (1<<number));
}
int get_bit(char byte, int number){

	int bit = 1;
	byte >>= number;
	return byte & bit;
}
char calculate_check_sum(char payload[]){

	int i,j,sum;
	char result = 0x00;
	for (j = 0; j < 8; j++){
		sum = 0;
		for (i = 0; i < MSGSIZE - 1; i++){
			sum = ( sum + get_bit(payload[i],j) ) % 2;
		}
		if (sum == 1) result = set_bit (result,j);
	}
	return result;
}
int fourth_task(char *filename, int speed, int delay){

	msg t;
	int fld, file_size, i, expected_ack = 0, received_ack,message_size = MSGSIZE-sizeof(int)-1, found_next_ack, MAX = 50;
	int next_frame2send = 0, BDP, window_size, number_of_frames;
	struct stat f_status;
	char *file = (char*)malloc(MAX * sizeof(char)), result;
	strcpy(file,"recv_");
	strcat(file,filename);

	printf("[SENDER] Sender starts.\n");
	printf("[SENDER] Filename=%s, task_index=%d, speed=%d, delay=%d\n", filename, 3, speed, delay);
	printf("[SENDER] Waiting for the transfer to finish..\n");

	init(HOST,PORT1);

	fld = open(filename,O_RDONLY);
	fstat(fld,&f_status);
	file_size = (int) f_status.st_size;
	BDP = speed * delay * 1000;
	/* window size =  numarul de cadre din legatura de date */
	window_size = BDP/(8*(MSGSIZE + 4));

	number_of_frames = file_size / (MSGSIZE - 5);
	if (file_size % (MSGSIZE - 5) != 0){
		number_of_frames++;
	}
	msg buffer[window_size];
	/* Un match pe buffer care retine pentru care
	 * pachete s-a primit confimare */
	int mirror[window_size];

	/* Initial trimit numarul de cadre pe care le va
	 * receptiona receiverul impreuna cu window_size-ul
	 * si numele fisierului de output */
	memset(&t,0,sizeof(msg));
	t.len = strlen(file);
	memcpy(t.payload,&number_of_frames,4);
	memcpy(t.payload+sizeof(int),&window_size,4);
	memcpy(t.payload+2*sizeof(int),file,t.len);
	result = calculate_check_sum(t.payload);
	memcpy(t.payload+MSGSIZE-1,&result,1);
	send_message(&t);
	while (recv_message_timeout(&t,max(1000,2*delay)) == -1){
		while( send_message(&t) < 0){ }
	}
	/* Senderul trimite o fereastra de cadre pentru
	 * a mentine ocupata legatura de date*/
	for (i = 0; i < window_size; i++){

		memset(&t,0,sizeof(msg));
		t.len = read(fld,t.payload,message_size);
		memcpy(t.payload+message_size,&next_frame2send,4);
		result = calculate_check_sum(t.payload);
		memcpy(t.payload+MSGSIZE-1,&result,1);
		buffer[i] = t;
		mirror[i] = -1;
		send_message(&t);
		next_frame2send++;
	}

	number_of_frames--;

	while (1){
		if (recv_message_timeout (&t, max(1000,2*delay)) == -1){
			/* Daca s-a intrat in time-out Senderul retrimite toate
			 * pachetele pentru care nu a primit confimare */
			for (i = expected_ack % window_size; i < window_size; i++){
				if (mirror[i] == -1){
					send_message (&buffer[i]);
				}
			}
			for (i = 0; i < expected_ack % window_size; i++){
				if (mirror[i] == -1){
					send_message (&buffer[i]);
				}
			}
		}else{
			memcpy (&received_ack, t.payload+message_size, 4);
			/* In cazul in care pachetul pe care il asteptam este cel de final si
			 * s-a primit confimare pentru acesta, se incheie transferul */
			if ( (received_ack == number_of_frames) && (expected_ack == number_of_frames)){
				break;
			}else{

				if (received_ack != expected_ack){
					/* Daca s-a primit un ACK si nu este cel pe care il asteptam
					 * marchez cadrul ca fiind receptionat */
					mirror[received_ack % window_size] = 1;
				}else{
					/* Daca ACK ul primit este cel pe care il asteptam
					 * caut prin bufferul rotit cate pachete pe langa acesta au mai fost
					 * confirmate, pentru a sti cate pachete trebuie trimise */
					memset(&t,0,sizeof(msg));
					t.len = read (fld, t.payload, message_size);
					if (t.len > 0){
						/* Inca au mai fost cadre de citit din fisier
						 * Sunt puse cadrele in buffer si marcate ca fiind neconfirmate */
						memcpy (t.payload+message_size, &next_frame2send, 4);
						result = calculate_check_sum(t.payload);
						memcpy(t.payload+MSGSIZE-1,&result,1);
						buffer [received_ack % window_size] = t;
						mirror[received_ack % window_size] = -1;
						send_message (&t);
						next_frame2send ++;
					}else{
						mirror[received_ack % window_size] = 1;
					}
					expected_ack ++;

					found_next_ack = 0;
					for (i = expected_ack % window_size; i < window_size; i++){
						if (mirror[i] == 1 && found_next_ack == 0){
							memset(&t,0,sizeof(msg));
							t.len =  read (fld, t.payload, message_size);
							if (t.len > 0){
								/* Inca au mai fost cadre de citit din fisier
								* Acestea sunt puse in buffer si marcate ca fiind neconfirmate */
								memcpy (t.payload+message_size, &next_frame2send, 4);
								result = calculate_check_sum(t.payload);
								memcpy(t.payload+MSGSIZE-1,&result,1);
								buffer [i] = t;
								mirror[i] = -1;
								send_message (&t);
								next_frame2send ++;
							}
							expected_ack ++;
						}else{
							found_next_ack = 1;
							break;
						}
					}
					/* Trebuie reactualizat urmatorul ACK ce va fi receptionat */
					if (found_next_ack == 0){
						for (i = 0; i < received_ack % window_size; i++){
							if (mirror[i] == 1 && found_next_ack == 0){
								memset(&t,0,sizeof(msg));
								t.len =   read (fld, t.payload, message_size);
								if (t.len > 0){
									/* Inca au mai fost octeti de citit din fisier
									* ii pun in buffer si marchez cadrul ca fiind neconfirmat */
									memcpy (t.payload+message_size, &next_frame2send, 4);
									result = calculate_check_sum(t.payload);
									memcpy(t.payload+MSGSIZE-1,&result,1);
									buffer [i] = t;
									mirror[i] = -1;
									send_message (&t);
									next_frame2send ++;
								}
								expected_ack ++;
							}else{
								found_next_ack = 1;
								break;
							}
						}
					}
					/* Daca urmatorul ACK depaseste numarul de cadre se incheie transferul */
					if (expected_ack > number_of_frames){
						break;
					}
				}

			}

		}
	}
	printf("[SENDER] Transfer finished! :D\n");
	close(fld);
	free(file);
	return 0;

}
int parity_bit_number(int number){

	int init = 2;
	while(number+init > pow(2,init)){
		init++;
	}
	return init;
}
void calculate_parity_bits(char *new_payload){

	int powers[11] = {0,1,3,7,15,31,63,127,255,511,1023},sum = 0;
	int i, k, j, count;
	for (i = 0; i < 8; i++){
		for ( k = 0; k < 11; k++){
			count = powers[k]+1;
			sum = 0;
			for( j = powers[k]; j < MSGSIZE;j++){
				sum+=get_bit(new_payload[j],i);
				count--;
				if (count == 0){
					count = powers[k]+1;
					j = j+count;
				}
			}
			sum = sum % 2;
			if (sum == 1)
				new_payload[powers[k]] = set_bit(new_payload[powers[k]],i);
		}
	}

}
int is_power_of2(int j){

	int i, powers[11] = {1,2,4,8,16,32,64,128,256,512,1024};
	for ( i = 0; i < 11; i++ ){
		if (powers[i]-1 == j){
			return 1;
		}
		if (powers[i]-1 > j) return 0;
	}
	return 0;
}
void form_hamming(char *payload, char  *new_payload){

	int new_payload_size = MSGSIZE;
	int i,i_payload = 0,j;
	memset(new_payload,0,MSGSIZE);
	for ( i = 0; i < 8; i++){
		i_payload = 0;
		for (j = 0; j < new_payload_size; j++){

			if (is_power_of2(j) != 1){
				if (get_bit(payload[i_payload],i) == 1){
					new_payload[j] = set_bit(new_payload[j],i);

				}
				i_payload++;
			}
		}
	}
	calculate_parity_bits(new_payload);
}
void extract_hamming(char* new_payload, char* payload, int number_of_parity_bits){

	int payload_size = MSGSIZE-number_of_parity_bits, new_payload_size = MSGSIZE;
	int i,i_payload = 0,j;
	memset(payload,0,payload_size);
	for ( i = 0; i < 8; i++){
		i_payload = 0;
		for (j = 0; j < new_payload_size; j++){
			if (is_power_of2(j) != 1){
				if (get_bit(new_payload[j],i) == 1){
					payload[i_payload] = set_bit(payload[i_payload],i);
				}
				i_payload++;
			}
		}
	}
}
int fifth_task(char *filename, int speed, int delay){

	msg t;
	int fld, file_size, i, expected_ack = 0, received_ack, found_next_ack, MAX = 50;
	int next_frame2send = 0, BDP, window_size, number_of_frames,parity_bits_number = parity_bit_number(MSGSIZE);
	int message_size = MSGSIZE-sizeof(int)-parity_bits_number;
	struct stat f_status;
	char *file = (char*)malloc(MAX * sizeof(char));
	char *new_payload = (char *)malloc(MSGSIZE*sizeof(char));
	char *payload= (char *)malloc((message_size+sizeof(int))*sizeof(char));
	strcpy(file,"recv_");
	strcat(file,filename);

	printf("[SENDER] Sender starts.\n");
	printf("[SENDER] Filename=%s, task_index=%d, speed=%d, delay=%d\n", filename, 4, speed, delay);
	printf("[SENDER] Waiting for the transfer to finish..\n");

	init(HOST,PORT1);

	fld = open(filename,O_RDONLY);
	fstat(fld,&f_status);
	file_size = (int) f_status.st_size;
	BDP = speed * delay * 1000;
	/* window size =  numarul de cadre din legatura de date */
	window_size = BDP/(8*(MSGSIZE + 4));

	number_of_frames = file_size / message_size;
	if (file_size % message_size != 0){
		number_of_frames++;
	}
	msg buffer[window_size];
	/* Un match pe buffer care retine pentru care
	 * pachete s-a primit confimare */
	int mirror[window_size];

	/* Initial trimit numarul de cadre pe care le va
	 * receptiona receiverul impreuna cu window_size-ul
	 * si numele fisierului de output */
	memset(&t,0,sizeof(msg));
	t.len = strlen(file);
	memcpy(t.payload,&number_of_frames,4);
	memcpy(t.payload+sizeof(int),&window_size,4);
	memcpy(t.payload+2*sizeof(int),file,t.len);
	form_hamming(t.payload,new_payload);
	memcpy(t.payload,new_payload,MSGSIZE);
	send_message(&t);
	while (recv_message(&t) == -1){
		
		while( send_message(&t) < 0){ }
	}
	/* Senderul trimite o fereastra de cadre pentru
	 * a mentine ocupata legatura de date*/
	for (i = 0; i < window_size; i++){

		memset(&t,0,sizeof(msg));
		t.len = read(fld,t.payload,message_size);
		memcpy(t.payload+message_size,&next_frame2send,4);
		form_hamming(t.payload,new_payload);
		memcpy(t.payload,new_payload,MSGSIZE);
		buffer[i] = t;
		mirror[i] = -1;
		send_message(&t);
		next_frame2send++;
	}

	number_of_frames--;

	while (1){
		if (recv_message_timeout (&t,max(1000,2*delay)) == -1){
			/* Daca s-a intrat in time-out Senderul retrimite toate
			 * pachetele pentru care nu a primit confimare */
			for (i = expected_ack % window_size; i < window_size; i++){
				if (mirror[i] == -1){
					send_message (&buffer[i]);
				}
			}
			for (i = 0; i < expected_ack % window_size; i++){
				if (mirror[i] == -1){
					send_message (&buffer[i]);
				}
			}
		}else{
			
			extract_hamming(t.payload,payload,parity_bits_number);
			memcpy (&received_ack, payload+message_size, 4);
			/* In cazul in care pachetul pe care il asteptam este cel de final si
			 * s-a primit confimare pentru acesta, se incheie transferul */
			if ( (received_ack == number_of_frames) && (expected_ack == number_of_frames)){
				break;
			}else{

				if (received_ack != expected_ack){
					/* Daca s-a primit un ACK si nu este cel pe care il asteptam
					 * marchez cadrul ca fiind receptionat*/
					mirror[received_ack % window_size] = 1;
				}else{
					/* Daca ACK ul primit este cel pe care il asteptam
					 * caut prin bufferul rotit cate pachete pe langa acesta au mai fost
					 * confirmate, pentru a sti cate pachete trebuie trimise*/
					memset(&t,0,sizeof(msg));
					t.len = read (fld, t.payload, message_size);
					if (t.len > 0){
						/* Inca au mai fost cadre de citit din fisier
						* sunt pusi in buffer si marcati ca fiind neconfirmati */
						memcpy (t.payload+message_size, &next_frame2send, 4);
						form_hamming(t.payload,new_payload);
						memcpy(t.payload,new_payload,MSGSIZE);
						buffer [received_ack % window_size] = t;
						mirror[received_ack % window_size] = -1;
						send_message (&t);
						next_frame2send ++;
					}else{
						mirror[received_ack % window_size] = 1;
					}
					expected_ack ++;

					found_next_ack = 0;
					for (i = expected_ack % window_size; i < window_size; i++){
						if (mirror[i] == 1 && found_next_ack == 0){
							memset(&t,0,sizeof(msg));
							t.len =  read (fld, t.payload, message_size);
							if (t.len > 0){
								/* Inca au mai fost cadre de citit din fisier
								* sunt pusi in buffer si marcati ca fiind neconfirmati */
								memcpy (t.payload+message_size, &next_frame2send, 4);
								form_hamming(t.payload,new_payload);
								memcpy(t.payload,new_payload,MSGSIZE);
								buffer [i] = t;
								mirror[i] = -1;
								send_message (&t);
								next_frame2send ++;
							}
							expected_ack ++;
						}else{
							found_next_ack = 1;
							break;
						}
					}
					/* Trebuie reactualizat urmatorul ACK ce trebuie receptionat */
					if (found_next_ack == 0){
						for (i = 0; i < received_ack % window_size; i++){
							if (mirror[i] == 1 && found_next_ack == 0){
								memset(&t,0,sizeof(msg));
								t.len =   read (fld, t.payload, message_size);
								if (t.len > 0){
									/* Inca au mai fost octeti de citit din fisier
									* ii pun in buffer si il marchez ca fiind neconfirmat */
									memcpy (t.payload+message_size, &next_frame2send, 4);
									form_hamming(t.payload,new_payload);
									memcpy(t.payload,new_payload,MSGSIZE);
									buffer [i] = t;
									mirror[i] = -1;
									send_message (&t);
									next_frame2send ++;
								}
								expected_ack ++;
							}else{
								found_next_ack = 1;
								break;
							}
						}
					}
					/* Daca urmatorul ACK depaseste numarul de cadre se incheie transferul */
					if (expected_ack > number_of_frames){
						break;
					}
				}

			}

		}
	}
	printf("[SENDER] Transfer finished! :D\n");
	close(fld);
	free(file);
	free(new_payload);
	free(payload);
	return 0;

}
int main(int argc, char *argv[])
{
	char *filename;
	int task_index, speed, delay;
	task_index = atoi(argv[1]);
	filename = argv[2];
	speed = atoi(argv[3]);
	delay = atoi(argv[4]);
	switch(task_index){
	
		case 0: first_task(filename, speed, delay); break;
		case 1: second_task(filename, speed, delay); break;
		case 2: third_task(filename,speed,delay); break;
		case 3: fourth_task(filename, speed, delay); break;
		case 4: fifth_task(filename, speed, delay); break;
	}
	return 0;
}
