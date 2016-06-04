#ifndef POLITICAS_H
#define	POLITICAS_H

#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "bcp.h"
#include "bcpList.h"

typedef enum POLITICA_ESC{
    POL_RR,
    POL_RANDOM,
    POL_FCFS,
    POL_SJF,
    POL_FP
} POLITICA_ESC;

typedef struct rr_t{
    int quantum;
    bcpList_t* fifo;
    int pos;
}rr_t;

typedef struct fp_t{
	int tam;
    struct politica_t** filas;
}fp_t;

typedef struct fcfs_t{
    bcpList_t* fifo;
}fcfs_t;

typedef struct random_t{
    bcpList_t* lista;
}random_t;

typedef struct politica_t{
    POLITICA_ESC politica;
    union{
        rr_t* rr;
        fp_t* fp;
        fcfs_t* fcfs;
	random_t* random;
    }param;
    bcp_t* (*escalonar)(struct politica_t*);
    void (*tick)(struct politica_t*);
    void (*novoProcesso)(struct politica_t*, bcp_t*);
    void (*fimProcesso)(struct politica_t*, bcp_t*);
    void (*desbloqueado)(struct politica_t*, bcp_t*);
}politica_t;

politica_t* POLITICA_criar(FILE* arqProcessos);
void POLITICA_imprimir(politica_t* politica);

static void DUMMY_tick(struct politica_t *p);
static void DUMMY_novo(struct politica_t *p, bcp_t* novoProcesso);
static void DUMMY_fim(struct politica_t *p, bcp_t* processoTerminado);
static void DUMMY_desbloqueado(struct politica_t* p, bcp_t* processoDesbloqueado);

static void RR_tick(struct politica_t *p);
static void RR_novoProcesso(struct politica_t *p, bcp_t* novoProcesso);
static bcp_t* RR_escalonar(struct politica_t *p);
static void RR_fimProcesso(struct politica_t *p, bcp_t* processo);
static politica_t* POLITICARR_criar(FILE* arqProcessos);

static void FP_novoProcesso(struct politica_t *p, bcp_t* novoProcesso);
static void FP_fimProcesso(struct politica_t *p, bcp_t* processo);
static bcp_t* FP_escalonar(struct politica_t *p);
static politica_t* POLITICAFP_criar(FILE* arqProcessos);

static void FCFS_novoProcesso(struct politica_t *p, bcp_t* novoProcesso);
static void FCFS_fimProcesso(struct politica_t *p, bcp_t* processo);
static bcp_t* FCFS_escalonar(struct politica_t *p);
static politica_t* POLITICAFCFS_criar(FILE* arqProcessos);
static politica_t* POLITICAFCFS_criar(FILE* arqProcessos);

static void RANDOM_novoProcesso(struct politica_t *p, bcp_t* novoProcesso);
static void RANDOM_fimProcesso(struct politica_t *p, bcp_t* processo);
static bcp_t* RANDOM_escalonar(struct politica_t *p);
static politica_t* POLITICARANDOM_criar(FILE* arqProcessos);

static politica_t* POLITICASJF_criar(FILE* arqProcessos);
#endif	/* POLITICAS_H */

