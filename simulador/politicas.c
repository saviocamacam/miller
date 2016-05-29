#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "politicas.h"
#include "bcp.h"
#include "bcpList.h"

extern bcpList_t *bloqueados;
extern bcpList_t *prontos;
extern bcp_t* executando;
extern char *diagramaDeEventos;
extern uint64_t tamStringDiagrama;


/*
 * Funções DUMMY são aquelas que não fazem nada... 
 * 
 * Existem políticas que não tomam ações em determinados pontos da simulação. Nestes casos
 * usa-se as rotinas DUMMY para não ter que tratar essas casos no loop de simulação.
*/

void DUMMY_tick(struct politica_t *p){
    return;
}

void DUMMY_novo(struct politica_t *p, bcp_t* novoProcesso){
    return;
}

void DUMMY_fim(struct politica_t *p, bcp_t* processoTerminado){
    return;
}

void DUMMY_desbloqueado(struct politica_t* p, bcp_t* processoDesbloqueado){
    return;
}

/*
 * Round-Robin
 * 
 * Os callbacks abaixo implementam a política round-robin para escalonamento de processos
 * 
 */

void RR_tick(struct politica_t *p){
    if(executando){
        //decrementar o tempo restante deste processo
        executando->timeSlice--;
        if(executando->timeSlice <= 0){
            //se o tempo do processo acabou, inserir o processo atual na lista de prontos
            LISTA_BCP_inserir(prontos, executando);
            //remover o processo atual de execução
            executando = NULL;
        }
    }
}

void RR_novoProcesso(struct politica_t *p, bcp_t* novoProcesso){
    //quando um novo processo chega, ele é inserido na fila round robin
    LISTA_BCP_inserir(p->param.rr->fifo, novoProcesso);
}

bcp_t* RR_escalonar(struct politica_t *p){
    bcp_t* ret;
    int nBloqueados = 0;
    
    //Se não há processos na fila round-robin, retornar nenhum
    if(p->param.rr->fifo->tam == 0)
        return NULL;
    
    //testar todos os processos da fila round-robin a partir da posição atual
    while(nBloqueados < p->param.rr->fifo->tam){
    
        //verificar é necessário apontar para o primeiro elemento novamente
        if(p->param.rr->pos >= p->param.rr->fifo->tam){
            p->param.rr->pos = 0;
        }

        ret = p->param.rr->fifo->data[p->param.rr->pos];

        //verificar se o atual da fila round-robin está bloqueado
        if(LISTA_BCP_buscar(bloqueados, ret->pid) != LISTA_N_ENCONTRADO){
            //Se estiver, testar o próximo! 
            nBloqueados++;
            ret = NULL;
        }
        else{
            //retornar o processo para ser executado!
            LISTA_BCP_remover(prontos, ret->pid);
            ret->timeSlice = p->param.rr->quantum;
            break;
        }
        
        p->param.rr->pos++;
    }
    
    p->param.rr->pos++;
    
    return ret;
}

void RR_fimProcesso(struct politica_t *p, bcp_t* processo){
    //Quando um processo termina, removê-lo da fila round-robin
    LISTA_BCP_remover(p->param.rr->fifo, processo->pid);
}

politica_t* POLITICARR_criar(FILE* arqProcessos){
    politica_t* p;
    char* s;
    rr_t* rr;
    
    p = malloc(sizeof(politica_t));
    
    p->politica = POL_RR;
    
    //Ligar os callbacks com as rotinas RR
    p->escalonar = RR_escalonar;
    p->tick = RR_tick;
    p->novoProcesso = RR_novoProcesso;
    p->fimProcesso = RR_fimProcesso;
    p->desbloqueado = DUMMY_desbloqueado;
    
    //Alocar a struct que contém os parâmetros para a política round-robin
    rr = malloc(sizeof(rr_t));
    s = malloc(sizeof(char) * 10);
    
    fgets(s, 10, arqProcessos);
    
    //inicializar a estrutura de dados round-robin
    rr->quantum = atoi(s);
    rr->fifo = LISTA_BCP_criar();
    rr->pos = 0;
    
    //Atualizar a política com os parâmetros do escalonador
    p->param.rr = rr;
    
    free(s);
    
    return p;
    
}

/*
 * Fila de Prioridade
 */

politica_t* POLITICAFP_criar(FILE* arqProcessos){
    return NULL;
}

/*
 * FCFS (First Came First Served)
 */


void FCFS_novoProcesso(struct politica_t *p, bcp_t* novoProcesso){
    //quando um novo processo chega, ele é inserido na fila round robin
    LISTA_BCP_inserir(p->param.fcfs->fifo, novoProcesso);
}

void FCFS_fimProcesso(struct politica_t *p, bcp_t* processo){
    //Quando um processo termina, removê-lo da fila round-robin
    LISTA_BCP_remover(p->param.fcfs->fifo, processo->pid);
}

bcp_t* FCFS_escalonar(struct politica_t *p){
    bcp_t* ret;
    bcp_t* aux;

    int nBloqueados = 0;
    
    //Se não há processos na fila fcfs, retornar nenhum
    if(p->param.fcfs->fifo->tam == 0 || LISTA_BCP_vazia(prontos))
        return NULL;

	ret = prontos->data[nBloqueados++];

	// procurar na lista de prontos
	while( nBloqueados < prontos->tam )
	{
		aux = prontos->data[nBloqueados];
		if( ret->entrada > aux->entrada )
			ret = aux;
		nBloqueados++;
	}

	LISTA_BCP_remover(prontos, ret->pid);

    return ret;
}

politica_t* POLITICAFCFS_criar(FILE* arqProcessos){
    politica_t* p;
    char* s;
    fcfs_t* fcfs;
    
    p = malloc(sizeof(politica_t));
    
    p->politica = POL_FCFS;
    
    //Ligar os callbacks com as rotinas RR
    p->escalonar = FCFS_escalonar;
    p->tick = DUMMY_tick;
    p->novoProcesso = FCFS_novoProcesso;
    p->fimProcesso = FCFS_fimProcesso;
    p->desbloqueado = DUMMY_desbloqueado;
    
    //Alocar a struct que contém os parâmetros para a política round-robin
    fcfs = malloc(sizeof(fcfs_t));
    
    //inicializar a estrutura de dados fcfs
    fcfs->fifo = LISTA_BCP_criar();
    
    //Atualizar a política com os parâmetros do escalonador
    p->param.fcfs = fcfs;
    
    return p;
}

/*
 * Random
 */

void RANDOM_novoProcesso(struct politica_t *p, bcp_t* novoProcesso){
    //quando um novo processo chega, ele é inserido na fila round robin
    LISTA_BCP_inserir(p->param.random->lista, novoProcesso);
}

void RANDOM_fimProcesso(struct politica_t *p, bcp_t* processo){
    //Quando um processo termina, removê-lo da fila round-robin
    LISTA_BCP_remover(p->param.random->lista, processo->pid);
}

bcp_t* RANDOM_escalonar(struct politica_t *p){

    //Se não há processos na fila random retornar null
    if(LISTA_BCP_vazia(prontos))
        return NULL;

	// calcula o indice de maneira pseudorandomica
	srand(time(NULL));
	int index = rand() % prontos->tam;

	bcp_t *ret;
	ret = prontos->data[index];
	LISTA_BCP_remover(prontos, ret->pid);

    return ret;
}

politica_t* POLITICARANDOM_criar(FILE* arqProcessos){
    politica_t* p;
    random_t* random;
    
    p = malloc(sizeof(politica_t));
    
    p->politica = POL_RANDOM;
    
    //Ligar os callbacks com as rotinas RR
    p->escalonar = RANDOM_escalonar;
    p->tick = DUMMY_tick;
    p->novoProcesso = RANDOM_novoProcesso;
    p->fimProcesso = RANDOM_fimProcesso;
    p->desbloqueado = DUMMY_desbloqueado;
    
    //Alocar a struct que contém os parâmetros para a política round-robin
    random = malloc(sizeof(random_t));
    
    //inicializar a estrutura de dados fcfs
    random->lista = LISTA_BCP_criar();
    
    //Atualizar a política com os parâmetros do escalonador
    p->param.random = random;
    
    return p;
}

/*
 * Gerais
 */

politica_t* POLITICA_criar(FILE* arqProcessos){
    char* str;
    
    str = malloc(sizeof(char) * 20);
    
    fgets(str, 20, arqProcessos);
    
    //*(strstr(str, "\n")) = '\0';
    
    politica_t* p;
    p = malloc(sizeof(politica_t));
    
    if(!strncmp(str, "sjf", 3)){
        p->param.rr = NULL;
        p->politica = POL_SJF;
        p->escalonar = NULL;
        p->tick = DUMMY_tick;
        p->novoProcesso = DUMMY_novo;
        p->fimProcesso = DUMMY_fim;
        p->desbloqueado = DUMMY_desbloqueado;
    }
    
    if(!strncmp(str, "fcfs", 4)){
		free(p);
        p = POLITICAFCFS_criar(arqProcessos);
    }
    
    if(!strncmp(str, "random",6)){
		free(p);
        p = POLITICARANDOM_criar(arqProcessos);
    }
    
    if(!strncmp(str, "rr", 2)){
        free(p);
        p = POLITICARR_criar(arqProcessos);
    }
    
    if(!strncmp(str, "fp", 2)){
        free(p);
        p = POLITICAFP_criar(arqProcessos);
    }
    
    free(str);
    
    return p;
}

void POLITICA_imprimir(politica_t* politica){
    const char* pol;
    
    if(politica->politica == POL_FCFS)
        pol = "FCFS";
    if(politica->politica == POL_FP)
        pol = "FP";
    if(politica->politica == POL_RANDOM)
        pol = "RANDOM";
    if(politica->politica == POL_RR)
        pol = "RR";
    if(politica->politica == POL_SJF)
        pol = "SJF";

    printf("política de escalonamento: %s\n", pol);
    if(politica->politica == POL_RR)
        printf("\tquantum: %d\n", politica->param.rr->quantum);
    
    return;
}
