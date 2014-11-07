#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>

struct Args {
    char *remote_addr;
    int is_server;
    int port;
};

struct Args args;

int main(int argc, char *argv[])
{
    struct rdma_event_channel *evch;
    struct rdma_cm_id *server_id;
    struct rdma_cm_id *client_id;
    struct rdma_cm_event *event;
    struct rdma_conn_param conn_param;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_mr *mr;
    struct ibv_send_wr snd_wr;
    struct ibv_recv_wr rcv_wr;
    struct ibv_sge sge;
    struct ibv_wc wc;
    struct ibv_qp_init_attr attr = {
        .cap = {
            .max_send_wr = 32,
            .max_recv_wr = 32,
            .max_send_sge = 1,
            .max_recv_sge = 1,
            .max_inline_data = 64
        },
        .qp_type = IBV_QPT_RC
    };

    char msg[256] = "Hello World";
    int msg_len = strlen(msg) + 1;
    struct sockaddr_in sin;

    args.remote_addr = "0.0.0.0";
    args.is_server = 1;
    args.port = 21234;

    /* Parameter parsing. */
    while (1) {
        int c;

        c = getopt(argc, argv, "c:p:");
        if (c == -1)
            break;

        switch (c) {
        case 'c':
            args.is_server = 0;
            args.remote_addr = optarg;
            break;
        case 'p':
            args.port = strtol(optarg, NULL, 0);
            break;
        default:
            perror("Invalid option");
            exit(-1);
        };
    }

    if (args.is_server) {
        if (!(evch = rdma_create_event_channel())) {
            perror("rdma_create_event_channel");
            exit(-1);
        }

        if (rdma_create_id(evch, &server_id, NULL, RDMA_PS_TCP)) {
            perror("rdma_create_id");
            exit(-1);
        }

        sin.sin_family = AF_INET;
        sin.sin_port = htons(args.port);
        sin.sin_addr.s_addr = htonl(INADDR_ANY);

        if (rdma_bind_addr(server_id, (struct sockaddr *)&sin)) {
            perror("rdma_bind_addr");
            exit(-1);
        }

        if (rdma_listen(server_id, 6)) {
            perror("rdma_listen");
            exit(-1);
        }

        if (rdma_get_cm_event(evch, &event)
            || event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
            perror("rdma_get_cm_event");
            exit(-1);
        }

        client_id = (struct rdma_cm_id *)event->id;

        if (!(pd = ibv_alloc_pd(client_id->verbs))) {
            perror("ibv_alloc_pd");
            exit(-1);
        }

        if (!(mr = ibv_reg_mr(pd, msg, 256,
                IBV_ACCESS_REMOTE_WRITE |
                IBV_ACCESS_LOCAL_WRITE |
                IBV_ACCESS_REMOTE_READ))) {
            perror("ibv_reg_mr");
            exit(-1);
        }

        if (!(cq = ibv_create_cq(client_id->verbs, 32, 0, 0, 0))) {
            perror("ibv_create_cq");
            exit(-1);
        }

        attr.send_cq = attr.recv_cq = cq;
        if (rdma_create_qp(client_id, pd, &attr)) {
            perror("rdma_create_qp");
            exit(-1);
        }

        memset(&conn_param, 0, sizeof conn_param);
        if (rdma_accept(client_id, &conn_param)) {
            perror("rdma_accept");
            exit(-1);
        }

        rdma_ack_cm_event(event);
        if (rdma_get_cm_event(evch, &event)
            || event->event != RDMA_CM_EVENT_ESTABLISHED) {
            perror("rdma_get_cm_event");
            exit(-1);
        }
        rdma_ack_cm_event(event);

        sge.addr = (uint64_t)msg;
        sge.length = msg_len;
        sge.lkey = mr->lkey;
        snd_wr.sg_list = &sge;
        snd_wr.num_sge = 1;
        snd_wr.opcode = IBV_WR_SEND;
        snd_wr.send_flags = IBV_SEND_SIGNALED;
        snd_wr.next = NULL;
        if (ibv_post_send(client_id->qp, &snd_wr, NULL)) {
            perror("ibv_post_send");
            exit(-1);
        }

        while (!ibv_poll_cq(cq, 1, &wc))
            ;
        if (wc.status != IBV_WC_SUCCESS) {
            perror("ibv_poll_cq");
            exit(-1);
        }
    }

    else {
        if (!(evch = rdma_create_event_channel())) {
            perror("rdma_create_event_channel");
            exit(-1);
        }

        if (rdma_create_id(evch, &client_id, NULL, RDMA_PS_TCP)) {
            perror("rdma_create_id");
            exit(-1);
        }

        sin.sin_family = AF_INET;
        sin.sin_port = htons(args.port);
        sin.sin_addr.s_addr = inet_addr(args.remote_addr);

        if (rdma_resolve_addr
            (client_id, NULL, (struct sockaddr *)&sin, 2000)) {
            perror("rdma_resolve_addr");
            exit(-1);
        }

        if (rdma_get_cm_event(evch, &event)
            || event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
            perror("rdma_get_cm_event");
            exit(-1);
        }
        rdma_ack_cm_event(event);

        if (rdma_resolve_route(client_id, 2000)) {
            perror("rdma_resolve_route");
            exit(-1);
        }

        if (rdma_get_cm_event(evch, &event)
            || event->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
            perror("rdma_get_cm_event");
            exit(-1);
        }
        rdma_ack_cm_event(event);

        if (!(pd = ibv_alloc_pd(client_id->verbs))) {
            perror("ibv_alloc_pd");
            exit(-1);
        }

        if (!(mr = ibv_reg_mr(pd, msg, 256,
                IBV_ACCESS_REMOTE_WRITE |
                IBV_ACCESS_LOCAL_WRITE |
                IBV_ACCESS_REMOTE_READ))) {
            perror("ibv_reg_mr");
            exit(-1);
        }

        if (!(cq = ibv_create_cq(client_id->verbs, 32, 0, 0, 0))) {
            perror("ibv_create_cq");
            exit(-1);
        }

        attr.send_cq = attr.recv_cq = cq;
        if (rdma_create_qp(client_id, pd, &attr)) {
            perror("rdma_create_qp");
            exit(-1);
        }

        sge.addr = (uint64_t)msg;
        sge.length = msg_len;
        sge.lkey = mr->lkey;
        rcv_wr.sg_list = &sge;
        rcv_wr.num_sge = 1;
        rcv_wr.next = NULL;
        if (ibv_post_recv(client_id->qp, &rcv_wr, NULL)) {
            perror("ibv_post_recv");
            exit(-1);
        }

        memset(&conn_param, 0, sizeof conn_param);
        if (rdma_connect(client_id, &conn_param)) {
            perror("rdma_connect");
            exit(-1);
        }

        if (rdma_get_cm_event(evch, &event)
            || event->event != RDMA_CM_EVENT_ESTABLISHED) {
            perror("rdma_get_cm_event");
            exit(-1);
        }
        rdma_ack_cm_event(event);

        while (!ibv_poll_cq(cq, 1, &wc)) ;
        if (wc.status != IBV_WC_SUCCESS) {
            perror("ibv_poll_cq");
            exit(-1);
        }

        fprintf(stdout, "Received %s \n", msg);
    }

    fprintf(stdout, "Done \n");
    return 0;
}
