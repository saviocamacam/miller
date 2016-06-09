#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "politicas.h"
#include "bcp.h"
#include "bcpList.h"
#include "logger.h"

extern bcpList_t *bloqueados;
extern bcpList_t *prontos;
extern bcp_t* executando;
extern char *diagramaDeEventos;
extern uint64_t tamStringDiagrama;
extern Log *logger;
extern uint64_t relogio;

#define BUFFER_LINHA 255 


/*
 * Funções DUMMY são aquelas que não fazem nada... 
 * 
 * Existem políticas que não tomam ações em determinados pontos da simulação. Nestes casos
 * usa-se as rotinas DUMMY para não ter que tratar essas casos no loop de simulação.
*/

static void DUMMY_tick(struct politica_t *p){
    return;
}

static void DUMMY_novo(struct politica_t *p, bcp_t* novoProcesso){
    return;
}

static void DUMMY_fim(struct politica_t *p, bcp_t* processoTerminado){
    return;
}

static void DUMMY_desbloqueado(struct politica_t* p, bcp_t* processoDesbloqueado){
    return;
}

/*
 * Round-Robin
 */

void RR_tick(struct politica_t *p){
    if(executando){
        //decrementar o tempo restante deste processo
        executando->timeSlice--;
        if(executando->timeSlice <= 0){

			// grava no log o bloqueio por quantum expirado
			char *content = malloc(sizeof(char)*BUFFER_LINHA);
			sprintf(content, "%" PRIu64 "\t%d\tQUANTUM_EX", relogio, executando->pid);
			recordEvent(logger, content, DIAGRAM_EVT);

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
    s = malloc(sizeof(char) * 20);
    
    fgets(s, 20, arqProcessos);
    
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

void FP_novoProcesso(struct politica_t *p, bcp_t* novoProcesso){
	int index = novoProcesso->prioridade - 1;

	politica_t *ptmp = p->param.fp->filas[index];
	ptmp->novoProcesso(ptmp, novoProcesso);

	ptmp = NULL;
}

void FP_fimProcesso(struct politica_t *p, bcp_t* processo){
	int index = processo->prioridade - 1;

	politica_t *ptmp = p->param.fp->filas[index];
	ptmp->fimProcesso(ptmp, processo);

	ptmp = NULL;
}

bcp_t* FP_escalonar(struct politica_t *p){
    
    //Se não há processos na fila fcfs, retornar nenhum
    if(LISTA_BCP_vazia(prontos))
        return NULL;

    int nBloqueados = 0;
    bcp_t* ret = NULL;
    bcp_t* aux = NULL;

	ret = prontos->data[nBloqueados++];

	// procurar o processo com maior prioridade (menor numero)
	while( nBloqueados < prontos->tam )
	{
		aux = prontos->data[nBloqueados];
		if( ret->prioridade < aux->prioridade )
			ret = aux;
		nBloqueados++;
	}

	aux = NULL;

	// pegar a politica correspondente daquele processo
	int index = ret->prioridade - 1;
	politica_t *ptmp = p->param.fp->filas[index];

	// escalonar a fila de acordo com a politica
	ret = ptmp->escalonar(ptmp);

	// remover o processo da lista de prontos
	LISTA_BCP_remover(prontos, ret->pid);

    return ret;
}

politica_t* POLITICAFP_criar(FILE* arqProcessos){
    politica_t* p;
    fp_t* fp;
    
    p = malloc(sizeof(politica_t));
    p->politica = POL_FP;
    
    //Ligar os callbacks com as rotinas RR
    p->escalonar = FP_escalonar;
    p->tick = DUMMY_tick;
    p->novoProcesso = FP_novoProcesso;
    p->fimProcesso = FP_fimProcesso;
    p->desbloqueado = DUMMY_desbloqueado;
    
    //Alocar a struct que contém os parâmetros para a política fp
    fp = malloc(sizeof(fp_t));
	//FIXME: remover numero magico
	fp->filas = malloc(sizeof(politica_t*) * 40);
	fp->tam = 0;

	char *linha = malloc((sizeof(char)*BUFFER_LINHA)+1);
	while ( !feof(arqProcessos) )
	{
		if (fgets(linha, BUFFER_LINHA, arqProcessos) == NULL)
			break;

		if(!strncmp(linha, "fcfs", 4))
		{
			*(fp->filas+fp->tam++) = POLITICAFCFS_criar(NULL);
		}
		else if(!strncmp(linha, "sjf", 3))
		{
			*(fp->filas+fp->tam++) = POLITICASJF_criar(NULL);
		}
		else if(!strncmp(linha, "random",6))
		{
			*(fp->filas+fp->tam++) = POLITICARANDOM_criar(NULL);
		} 
		else if(!strncmp(linha, "rr", 2))
		{
            // cria arquivo temporario para o param do RR
			FILE *tmpfp = tmpfile(); 
			if( tmpfp == NULL )
			{
				perror("Erro na abertura do arquivo!");
				exit(1);
			}
            // pega o numero entre ( )
			char *param = strtok((linha+3), ")");

			fprintf(tmpfp, "%s\n", param);
			rewind(tmpfp);

            // cria a politica RR com o arquivo temporario
			*(fp->filas+fp->tam++) = POLITICARR_criar(tmpfp);

			if(fclose(tmpfp))
			{
				perror("Erro no fechamento do arquivo!");
				exit(1);
			}
			tmpfp = NULL;
		}

	}

	free(linha);
	linha = NULL;

	//Atualizar a política com os parâmetros do escalonador
	p->param.fp = fp;

	return p;
}

/*
 * FCFS 
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
 * SJF
 */

void SJF_novoProcesso(struct politica_t *p, bcp_t* novoProcesso)
{
	// variavel que guarda o tempo total do Processo
	uint64_t tTotalProcesso = 0;

	// calcula o tempo total do processo
	int i=0;
	for( ; i < novoProcesso->nEventos; i++ )
	{
		if ( novoProcesso->eventos[i]->evento == EVT_BLOQUEIO )
			tTotalProcesso += novoProcesso->eventos[i]->tempo;
		else if ( novoProcesso->eventos[i]->evento == EVT_TERMINO )
			tTotalProcesso += novoProcesso->eventos[i]->tempo;
	}

	novoProcesso->tTotalProcesso = tTotalProcesso;
	
	// insere o processo na lista da politica
	LISTA_BCP_inserir(p->param.sjf->lista, novoProcesso);
}

void SJF_fimProcesso(struct politica_t *p, bcp_t* processo)
{
	LISTA_BCP_remover(p->param.sjf->lista, processo->pid);
}

bcp_t* SJF_escalonar(struct politica_t *p)
{
	if ( LISTA_BCP_vazia(prontos) )
		return NULL;

	int i = 0;
	bcp_t *ret = prontos->data[i++];
	bcp_t *aux;
	for ( ; i < prontos->tam; i++ )
	{
		aux = prontos->data[i];
		if ( aux->tTotalProcesso < ret->tTotalProcesso )
		{
			ret = aux;
			aux = NULL;
		}
	}

	LISTA_BCP_remover(prontos, ret->pid);

	return ret;
}

void SJF_tick(struct politica_t *p)
{
    if(executando){
        //decrementar o tempo restante deste processo
        executando->tTotalProcesso--;
    }
}

politica_t* POLITICASJF_criar(FILE* arqProcessos)
{
    politica_t* p;
    sjf_t* sjf;
    
    p = malloc(sizeof(politica_t));
    
    p->politica = POL_SJF;
    
    //Ligar os callbacks com as rotinas RR
    p->escalonar = SJF_escalonar;
    p->tick = SJF_tick;
    p->novoProcesso = SJF_novoProcesso;
    p->fimProcesso = SJF_fimProcesso;
    p->desbloqueado = DUMMY_desbloqueado;
    
    //Alocar a struct que contém os parâmetros para a política round-robin
    sjf = malloc(sizeof(sjf_t));
    
    //inicializar a estrutura de dados fcfs
    sjf->lista = LISTA_BCP_criar();
    
    //Atualizar a política com os parâmetros do escalonador
    p->param.sjf = sjf;
    
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

    if(!strncmp(str, "sjf", 3)){
		p = POLITICASJF_criar(arqProcessos);
    }
    
    if(!strncmp(str, "fcfs", 4)){
        p = POLITICAFCFS_criar(arqProcessos);
    }
    
    if(!strncmp(str, "random",6)){
        p = POLITICARANDOM_criar(arqProcessos);
    }
    
    if(!strncmp(str, "rr", 2)){
        p = POLITICARR_criar(arqProcessos);
    }
    
    if(!strncmp(str, "fp", 2)){
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
