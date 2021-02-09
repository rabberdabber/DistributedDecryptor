#include "common.h"

char * filename;
char * server_port;
char * server_ip;


void
send_file(int clientfd)
{
    int len;
    char buf[BUFSIZE+HDR_LEN];

    FILE *fp = fopen(filename, "r");

    struct Hdr * header,*error_header;

    header = create_pckt(0,DELIVERY);
    error_header = create_pckt(0,ERROR);

    if (fp)
    {

        while ((len = fread(buf+HDR_LEN, sizeof(char), BUFSIZE-(HDR_LEN+1), fp)) > 0)
        {
            /* data delivery */
            header->msg_type = htonl(DELIVERY);

            /* the length of the data + header*/
            header->tot_len = htonl(len + HDR_LEN);

            memcpy((void *)buf,(void *)header,HDR_LEN);
            send_to_host(clientfd, buf, len + HDR_LEN);
        }

        /*data -store*/
        header->msg_type = htonl(DONE);

        /* the length of the data + header*/
        header->tot_len = htonl(HDR_LEN);

        send_to_host(clientfd, (char *)header,HDR_LEN);

        fclose(fp);
    }

    else
    {
        perror("fopen");

        if (error_header)
        {
            send_to_host(clientfd,(char *)error_header,HDR_LEN);
        }
    }

    free(header);
    free(error_header);
}


void recv_file(int clientfd)
{
    char buf[BUFSIZE];
    size_t recv_amt,cmd,len;
    string final_str = "";

    struct Hdr * hdr;

    while(1)
    {

        recv_amt = recv_from_host(clientfd, buf, HDR_LEN);
        buf[recv_amt] = '\0';


        if(recv_amt != 0)
        {
            hdr = (struct Hdr *)buf;
            if (hdr->msg_type == htons(ERROR))
            {
                fprintf(stderr, "Client has encountered an Error\n");
                exit(EXIT_FAILURE);
            }

            cmd = ntohl(hdr->msg_type);

            if (cmd == DELIVERY)
            {
                len = ntohl(hdr->tot_len) - HDR_LEN;

                recv_amt = recv_from_host(clientfd, buf, len);
                buf[recv_amt] = '\0';

                string tmp(buf);

                final_str += tmp;
            }

            if (cmd == DONE)
            {
                break;
            }
            
        }

    }

    ofstream myfile;
    myfile.open("decrypted.txt");
    myfile << final_str;
    myfile.close();
   
}


void handle_client()
{
    int clientfd;
   
    if ((clientfd = open_clientfd(server_ip,atoi(server_port))) == -1)
    {
        exit(EXIT_FAILURE);
    }

    /* send the file */
    send_file(clientfd);

    recv_file(clientfd);

    close(clientfd);
    
}

int main(int argc, char *argv[])
{

    if (argc != 4)
    {
        fprintf(stderr, "usage: ./client <file_name> <server_ip> <server_port>\n");
        exit(EXIT_FAILURE);
    }

    filename = argv[1];
    server_ip = argv[2];
    server_port = argv[3];

    handle_client();

    return 0;
}
