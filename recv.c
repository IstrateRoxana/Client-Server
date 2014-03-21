#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include "lib.h"

int first_task(){

	msg r;
	int i, result,len;
	long number_of_frames;
	int  MAX = 50, fd;
	char *file = (char*)malloc(MAX * sizeof(char));

	init(HOST, PORT2);
	printf("[RECEIVER] Receiver starts.\n");
	printf("[RECEIVER] Task index=%d\n", 0);

	/* Initial voi primi numarul total de frame-uri */
	memset(&r,0,sizeof(msg));
	while( recv_message(&r) < 0 ){}
	/* Trimit ACK */
	number_of_frames = r.len;
	memcpy(&len,r.payload,4);
	memcpy(file,r.payload+4,len);
	fd = open(file,O_WRONLY | O_TRUNC | O_CREAT, 0666);
	result = send_message(&r);
	if  (result  < 0){
		perror("[RECEIVER] Receive error.\n");
		return -1;
	}
	for (i = 0; i < number_of_frames; i++){
		memset(&r,0,sizeof(msg));

		/* Astept ACK */
		if (recv_message(&r) != -1){

			write(fd,r.payload,r.len);
			/* Trimit inapoi ACK */
			result = send_message(&r);
			if  (result  < 0){
				perror("[RECEIVER] Receive error.\n");
				return -1;
			}
		}
	}
	printf("[RECEIVER] All done.\n");
	close(fd);
	free(file);
	return 0;
}
int second_task(){

	msg r;
	int number_of_frames,frame_expected = 0,this_frame,message_size = MSGSIZE - sizeof(int),MAX = 50;
	char *filename = (char *)malloc(MAX*sizeof(char));

	init(HOST, PORT2);
	printf("[RECEIVER] Receiver starts.\n");
	printf("[RECEIVER] Task index=%d\n", 1);

	/* Initial primesc un mesaj de stabilire a conditiilor, ce contine
	 * numarul de frame-uri pe care le va primi receiverul si numele
	 * fisierului in care se vor scrie mesajele primite
	 */
	while( recv_message(&r) < 0 ){}
	/* Receiverul trimite ACK chiar si pentru pachetul inital */
	memcpy(&number_of_frames,r.payload,4);
	memcpy(filename,r.payload+sizeof(int),r.len);
	send_message(&r);
	int fd = open(filename,O_WRONLY | O_TRUNC | O_CREAT, 0666);

	while (1){

		memset(&r,0,sizeof(msg));
		if (recv_message(&r) == -1){
			/* In mod normal nu ar trebui sa se ajunga aici */
		}else{
			memcpy(&this_frame,r.payload+message_size,4);
			if (this_frame == number_of_frames && frame_expected == number_of_frames){
				/* Am primit ultimul mesaj, se incheie transferul */
				send_message(&r);
				break;
			}else{
				if(frame_expected == this_frame){

					/* Daca un pachet este pierdut, receiverul poate primi alt frame fata de cel asteptat
					 * insa pachetele venite in alta ordine le discardeaza */
					write(fd,r.payload,r.len);
					frame_expected++;
					send_message(&r);
				}
			}
		}
	}
	printf("[RECEIVER] Transfer finished! :D\n");
	close(fd);
	free(filename);
	return 0;
}
void initialize(int mirror[], int n){
	int i;
	for (i = 0; i < n; i++){
		mirror [i] = -1;
	}
}
int third_task(){

	msg r;
	int number_of_frames, expected_frame = 0, received_frame,i,message_size=MSGSIZE-sizeof(int);
	int window_size, found_next_ack, MAX = 50;
	char *filename = (char *)malloc(MAX * sizeof(char));

	init(HOST, PORT2);

	printf("[RECEIVER] Receiver starts.\n");
	printf("[RECEIVER] Task index=%d\n", 2);

	/* Initial receiverul primeste un cadru din care
	 * descopera numarul de cadre pe care le va
	 * receptiona, in plus si window_size-ul si numele
	 * fisierului in care va scrie cadrele receptionate */
	memset(&r,0,sizeof(msg));
	while( recv_message(&r) < 0 ){}

	memcpy(&number_of_frames,r.payload,4);
	memcpy(&window_size,r.payload+sizeof(int),4);
	memcpy(filename,r.payload+2*sizeof(int),r.len);

	/* Trimit ACK pentru confirmarea pachetului intial */
	send_message(&r);

	int fld =  open(filename,O_WRONLY | O_TRUNC | O_CREAT, 0666);
	msg buffer[window_size];
	int mirror[window_size];
	initialize(mirror,window_size);
	number_of_frames--;

	while (1){
		recv_message (&r);
		memcpy (&received_frame, r.payload+message_size, 4);
		if (received_frame == number_of_frames && expected_frame == number_of_frames){
			/* Daca am primit ultimul pachet si este si cel asteptat se incheie transferul */
			send_message (&r);
			write (fld, r.payload, r.len);
			break;
		}else{
			if (received_frame != expected_frame){
				/* Daca, cadrul primit nu este cel asteptat, dar este unul valid
				 * il marchez ca fiind primit si il retin in buffer */
				buffer [received_frame % window_size] = r;
				mirror [received_frame % window_size] = 1;
				send_message (&r);
			}else{
				/* Daca este cadrul asteptat il scriu in fisier.
				 * Caut circular in buffer care pachete au mai fost primite
				 * pentru a nu considera Senderul ca trebuie sa le retrimita
				 * si actualizez expected_ack */
				write (fld, r.payload, r.len);
				if (received_frame == number_of_frames){
					break;
				}
				mirror[received_frame % window_size] = -1;
				send_message(&r);
				expected_frame ++;

				found_next_ack = 0;
				for (i = expected_frame % window_size; i < window_size; i++){
					if (mirror[i] == 1){
						r = buffer [i];
						write (fld, r.payload, r.len);
						expected_frame ++;
						mirror[i] = -1;
					}else{
						/* Am gasit urmatorul cadru neprimit */
						found_next_ack = 1;
						break;
					}
				}
				/* Daca nu am gasit urmatorul cadru neprimit
				 * continui prin a cauta in cealalata parte a bufferului */
				if (found_next_ack == 0){
					for (i = 0; i < received_frame % window_size; i++){
						if (mirror[i] == 1){
							r = buffer [i];
							write (fld, r.payload, r.len);
							expected_frame ++;
							mirror[i] = -1;
						}else{
							found_next_ack = 1;
							break;
						}
					}
				}
				/* Daca urmatorul cadru pe care il astept este mai
				 * mare decat numarul de cadre se incheie transferul */
				if (expected_frame > number_of_frames){
					break;
				}
			}

		}

	}
	printf("[RECEIVER] Transfer finished! :D\n");
	close(fld);
	free(filename);
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
char clear_bit(char byte, int number){

	return (byte = byte & (~(1<<number)));

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

int fourth_task(){

	msg r;
	int number_of_frames, expected_frame = 0, received_frame,i,message_size=MSGSIZE-sizeof(int)-1;
	int window_size, found_next_ack, MAX = 50;
	char *filename = (char *)malloc(MAX * sizeof(char)), result, received_check_sum;

	init(HOST, PORT2);

	printf("[RECEIVER] Receiver starts.\n");
	printf("[RECEIVER] Task index=%d\n", 3);

	/* Initial receiverul primeste un cadru din care
	 * descopera numarul de cadre pe care le va
	 * receptiona, window_size-ul si numele
	 * fisierului in care va scrie cadrele receptionate */
	memset(&r,0,sizeof(msg));
	while( recv_message(&r) < 0 ){}
	result = calculate_check_sum(r.payload);
	memcpy(&received_check_sum,r.payload+MSGSIZE-1,1);
	if (result == received_check_sum){

		memcpy(&number_of_frames,r.payload,4);
		memcpy(&window_size,r.payload+sizeof(int),4);
		memcpy(filename,r.payload+2*sizeof(int),r.len);
		/* Trimit ACK pentru confirmarea pachetului intial */
		send_message(&r);
	}
	int fld =  open(filename,O_WRONLY | O_TRUNC | O_CREAT, 0666);
	msg buffer[window_size];
	int mirror[window_size];

	initialize(mirror,window_size);
	number_of_frames--;

	while (1){
		recv_message (&r);
		memcpy (&received_frame, r.payload+message_size, 4);
		result = calculate_check_sum(r.payload);
		memcpy(&received_check_sum,r.payload+MSGSIZE-1,1);
		/* Daca nu este corect checksum-ul consider pachetul ca fiind nereceptionat */
		if (result == received_check_sum){
			if (received_frame == number_of_frames && expected_frame == number_of_frames){
				/* Daca am primit ultimul pachet si este si cel asteptat si in plus
				 * daca suma de control este corecta, se incheie transferul */
					send_message (&r);
					write (fld, r.payload, r.len);
					break;
			}else{
				if (received_frame != expected_frame){
					/* Daca cadrul primit nu este cel asteptat, dar este unul valid
					 * si are suma de control corecta
					 * il marchez ca fiind primit si il retin in buffer */

						buffer [received_frame % window_size] = r;
						mirror [received_frame % window_size] = 1;
						send_message (&r);

				}else{
						/* Daca este cadrul asteptat si suma de control este corecta il scriu in fisier.
						 * Caut circular in buffer care pachete au mai fost primite
						 * pentru a nu considera Senderul ca trebuie sa le retrimita
						 * si actualizez expected_ack */

						write (fld, r.payload, r.len);
						if (received_frame == number_of_frames){
							break;
						}
						mirror[received_frame % window_size] = -1;
						send_message(&r);
						expected_frame ++;

						found_next_ack = 0;
						for (i = expected_frame % window_size; i < window_size; i++){
							if (mirror[i] == 1){
								r = buffer [i];
								write (fld, r.payload, r.len);
								expected_frame ++;
								mirror[i] = -1;
							}else{
								/* Am gasit urmatorul cadru neprimit */
								found_next_ack = 1;
								break;
							}
						}
						/* Daca nu am gasit urmatorul cadru neprimit
						 * continui prin a cauta in cealalata parte a bufferului */
						if (found_next_ack == 0){
							for (i = 0; i < received_frame % window_size; i++){
								if (mirror[i] == 1){
									r = buffer [i];
									write (fld, r.payload, r.len);
									expected_frame ++;
									mirror[i] = -1;
								}else{
									found_next_ack = 1;
									break;
								}
							}
						}
						/* Daca urmatorul cadru pe care il astept este mai
						 * mare decat numarul de cadre se incheie transferul */
						if (expected_frame > number_of_frames){
							break;
						}

				}
			}
		}
	}
	printf("[RECEIVER] Transfer finished! :D\n");
	close(fld);
	free(filename);
	return 0;

}
int parity_bit_number(int number){

	int init = 2;
	while(number+init > pow(2,init)){
		init++;
	}
	return init;
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
void correct_hamming(char *new_payload){

	char *back_up = (char *)malloc(MSGSIZE*sizeof(char));
	int i, powers[11] = {0,1,3,7,15,31,63,127,255,511,1023}, sum = 0, ok = 0, k;
	memcpy(back_up,new_payload,MSGSIZE);
	// am setat pe 0 toti bitii de paritate
	for (i = 0; i < 8; i++){
		for (k = 0; k < 11; k++){
			back_up[powers[k]] = clear_bit(back_up[powers[k]],i);
		}
	}
	calculate_parity_bits(back_up);

	for (i = 0; i < 8; i++){	
		sum = 0; ok = 0;
		for (k = 0; k < 11; k++){
			
			if (get_bit(back_up[powers[k]],i) != get_bit(new_payload[powers[k]],i)){
				sum+=powers[k]+1;
				ok = 1;
			}
		}
		sum--;
		
		if (ok == 1){		
			if (get_bit(new_payload[sum],i) == 1){
				new_payload[sum] = clear_bit(new_payload[sum],i);
			}else{
				new_payload[sum] = set_bit(new_payload[sum],i);
			}

		}
	}
	free(back_up);
	
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
int fifth_task(){

	msg r;
	int number_of_frames, expected_frame = 0, received_frame,i;
	int window_size, found_next_ack, MAX = 50, number_of_parity_bits = parity_bit_number(MSGSIZE);
	int message_size=MSGSIZE-sizeof(int)-number_of_parity_bits;
	/* Se observa ca numarul de biti de paritate este constant
	 * si se incadreaza in doua char uri */
	char *filename = (char *)malloc(MAX * sizeof(char));
	char *new_payload= (char *)malloc((message_size+sizeof(int))*sizeof(char));

	init(HOST, PORT2);

	printf("[RECEIVER] Receiver starts.\n");
	printf("[RECEIVER] Task index=%d\n", 4);

	/* Initial receiverul primeste un cadru din care
	 * descopera numarul de cadre pe care le va
	 * receptiona, window_size-ul si numele
	 * fisierului in care va scrie cadrele receptionate */
	while( recv_message(&r) < 0 ){}
	correct_hamming(r.payload);
	extract_hamming(r.payload,new_payload,number_of_parity_bits);	
	memcpy(&number_of_frames,new_payload,4);
	memcpy(&window_size,new_payload+sizeof(int),4);
	memcpy(filename,new_payload+2*sizeof(int),r.len);
	int fld = open(filename,O_WRONLY | O_TRUNC | O_CREAT);
	/* Trimit ACK pentru confirmarea pachetului intial */
	send_message(&r);
	
	msg buffer[window_size];
	int mirror[window_size];
	initialize(mirror,window_size);
	number_of_frames--;

	while (1){
		
		recv_message (&r);
		correct_hamming(r.payload);
		extract_hamming(r.payload,new_payload,number_of_parity_bits);
		memcpy (&received_frame, new_payload+message_size, 4);
		if (received_frame == number_of_frames && expected_frame == number_of_frames){
			/* Daca am primit ultimul pachet si este si cel asteptat si in plus
			 * daca suma de control este corecta, se incheie transferul */				
				send_message (&r);
				write (fld, new_payload, r.len);
				break;
		}else{
			if (received_frame != expected_frame){
				/* Daca cadrul primit nu este cel asteptat, dar este unul valid
				 * si are suma de control corecta
				 * il marchez ca fiind primit si il retin in buffer */
					buffer [received_frame % window_size] = r;
					mirror [received_frame % window_size] = 1;
					send_message (&r);
			}else{
				/* Daca este cadrul asteptat si suma de control este corecta il scriu in fisier.
				 * Caut circular in buffer care pachete au mai fost primite
				 * pentru a nu considera Senderul ca trebuie sa le retrimita
				 * si actualizez expected_ack */
				write (fld, new_payload, r.len);
				if (received_frame == number_of_frames){
					break;
				}
				mirror[received_frame % window_size] = -1;
				send_message(&r);
				expected_frame ++;

				found_next_ack = 0;
				for (i = expected_frame % window_size; i < window_size; i++){
					if (mirror[i] == 1){
						r = buffer [i];
						correct_hamming(r.payload);
						extract_hamming(r.payload,new_payload,number_of_parity_bits);
						write (fld, new_payload, r.len);
						expected_frame ++;
						mirror[i] = -1;
					}else{
						/* Am gasit urmatorul cadru neprimit */
						found_next_ack = 1;
						break;
					}
				}
				/* Daca nu am gasit urmatorul cadru neprimit
				 * continui prin a cauta in cealalata parte a bufferului */
				if (found_next_ack == 0){
					for (i = 0; i < received_frame % window_size; i++){
						if (mirror[i] == 1){
							r = buffer [i];
							correct_hamming(r.payload);
							extract_hamming(r.payload,new_payload,number_of_parity_bits);
							write (fld, new_payload, r.len);
							expected_frame ++;
							mirror[i] = -1;
						}else{
							found_next_ack = 1;
							break;
						}
					}
				}
				/* Daca urmatorul cadru pe care il astept este mai
				 * mare decat numarul de cadre se incheie transferul */
				if (expected_frame > number_of_frames){
					break;
				}
			}
		}
	}
	printf("[RECEIVER] Transfer finished! :D\n");
	close(fld);
	free(filename);
	free(new_payload);
	return 0;

}
int main(int argc, char *argv[])
{
	int task_index;
	task_index = atoi(argv[1]);	
	switch(task_index){
		case 0: first_task(); break;
		case 1: second_task(); break;
		case 2: third_task(); break;
		case 3: fourth_task(); break;
		case 4: fifth_task(); break;
	}
	return 0;
}
