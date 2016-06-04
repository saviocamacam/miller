#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>


#include "arq_experimento.h"
#include "arq_processos.h"
#include "bcpList.h"
#include "intList.h"
#include "logger.h"
#include "erros.h"

#define DEBUG 
#undef DEBUG

// define o intervalo de tempo para calculo da vazao
#define TEMPO_VAZAO 	1000.0f
#define BUFFER_TERMINO	256
#define BUFFER_DIAGRAMA_EVT 4096
#define BUFFER_HEADER 255

//Estrutura contendo informações sobre o experimento
experimento_t *experimento = NULL;
//Estrutura contendo os processos sendo executados
arq_processos_t *processos = NULL;
//Lista de processos prontos
bcpList_t *prontos = NULL;
//Lista de processos bloqueados
bcpList_t *bloqueados = NULL;
//Lista de processos que ainda não foram instanciados
bcpList_t *novos = NULL;
//Ponteiro para o processo atual
bcp_t* executando = NULL;
//Relógio do sistema
uint64_t relogio;

long double tme = 0.0;
long double tmr = 0.0;
long double vazao = 0.0;

uint64_t qtdProcExecutados = 0;

Log *logger;

int main(int argc, char** argv) {
    
    int i;
    
    if(argc != 2){
        fprintf(stderr, "uso: %s arquivo_de_experimento\n", argv[0]);
        exit(ARGUMENTOS_INVALIDOS);
    }
    
    //Ler arquivo de experimento
    experimento = EXPERIMENTO_ler(argv[1]);
    
    //Tirar \n do nome do arquivo de processos
    *(strstr(experimento->arq_processos, "\n")) = '\0';
    
    //Ler arquivo de processos
    processos = PROCESSOS_ler(experimento->arq_processos);
    
    //Criar as filas de processos
    prontos = LISTA_BCP_criar();
    bloqueados = LISTA_BCP_criar();
    novos = LISTA_BCP_criar();

	logger = createLog();
    
    //Inserir todoos processos do arquivo de processos na lista de novos
    for(i = 0; i < processos->nProcessos; i++){
        LISTA_BCP_inserir(novos, processos->processos[i]);
    }
    
    relogio = 0;
    
    uint64_t trocas_de_contexto = 0;
    uint64_t tempo_ocioso = 0;
    
    
    //Executar a simulação enquanto as listas não são vazias ou há pelo menos um processo executando
    while(!LISTA_BCP_vazia(prontos) 
            || !LISTA_BCP_vazia(bloqueados) 
            || !LISTA_BCP_vazia(novos)
            || executando != NULL){

        //Isto é pra evitar o laço que somente faz relogio++ (compensa!)
        if((bloqueados->tam == 0) && (prontos->tam == 0) && (novos->tam > 0) && (!executando)){
                tempo_ocioso += novos->data[0]->entrada - relogio;
                relogio = novos->data[0]->entrada;
        }
        
        //verificar se algum processo novo chegou neste momento
        if(novos->data[0]->entrada == relogio){
            LISTA_BCP_inserir(prontos, novos->data[0]);
            experimento->politica->novoProcesso(experimento->politica, novos->data[0]);
            LISTA_BCP_remover(novos, novos->data[0]->pid);
        }
        
        //Se há um processo executando...
        if(executando){
            //contabilizar mais um tempo executado no processo atual
            executando->tempoExecutado++;
            
            //verificar se é hora de um evento!
            if(executando->eventos[executando->proxEvento]->tempo == executando->tempoExecutado){
                
                //É um evento de término?
                if(executando->eventos[executando->proxEvento]->evento == EVT_TERMINO){

                    //invocar callback para o final de processo.
                    experimento->politica->fimProcesso(experimento->politica, executando);
                    
                    //salvar o momento da última execução
                    executando->tUltimaExec = relogio;

					// contabiliza o tmr desse processo
					tmr += (executando->tUltimaExec - executando->tPrimeiraExec );

					// incrementa o contador de processos concluidos
					qtdProcExecutados++;

					// grava o processo terminado
					char *tmppid = (char*)malloc(sizeof(char)*BUFFER_TERMINO);
					sprintf( tmppid, "%d ", executando->pid );
					recordEvent( logger, tmppid, SEQUENC_TER );
					free(tmppid);
					tmppid = NULL;

					// grava o evento no diagrama de Eventos
					char *tmpdiagrama = (char*)malloc(sizeof(char)*BUFFER_DIAGRAMA_EVT);
					sprintf(tmpdiagrama, "%" PRIu64 "\t%d\tTERMINO", relogio, executando->pid);
					recordEvent( logger, tmpdiagrama, DIAGRAM_EVT );
					free(tmpdiagrama);
                    
                    //Destruir o BCP do processo.
                    BCP_destruir(executando);
                    
                    //retirar o processo de execução
                    executando = NULL;
                }
                else{
					//TODO: implementar novos eventos para o diagrama

                    //Se não for evento de término, é um evento de bloqueio
                    executando->proxEvento++;
                    //O próximo evento é um desbloqueio! Pega o tempo que ficou bloqueado
                    executando->tempoBloqueio = executando->eventos[executando->proxEvento]->tempo + 1;
                    
                    //Apontar para o próximo evento
                    executando->proxEvento++;
                    
                    //Bloquear processo.
                    LISTA_BCP_inserir(bloqueados, executando);

					// grava o evento no diagrama de Eventos
					char *tmpdiagrama = (char*)malloc(sizeof(char)*BUFFER_DIAGRAMA_EVT);
					sprintf(tmpdiagrama, "%" PRIu64 "\t%d\tBLOQUEIO", relogio, executando->pid);
					recordEvent( logger, tmpdiagrama, DIAGRAM_EVT );
					free(tmpdiagrama);

                    //retirar processo de execução
                    executando = NULL;
                }
            }
            
        }
        
        //Executar o que tiver que ser executado a cada tick do relógio (callback)
        experimento->politica->tick(experimento->politica);
        
        //Atualizar o tempo de bloqueio de todos os processos bloqueados
        for(i = 0; i < bloqueados->tam; i++){
            bloqueados->data[i]->tempoBloqueio--;

            //se um processo deve ser desbloqueado
            if(bloqueados->data[i]->tempoBloqueio <= 0){
                bcp_t* p;
                p = bloqueados->data[i];
                //insere na fila de prontos
                LISTA_BCP_inserir(prontos, bloqueados->data[i]);
                //retira da fila de bloqueados
                LISTA_BCP_remover(bloqueados, bloqueados->data[i]->pid);
                //Invocar callback de desbloqueio de processo
                experimento->politica->desbloqueado(experimento->politica, p);
            }

        }

        //Se não há processo executando...
        if(!executando){
            //Invocar o callback de escalonamento para escolher um processo para ocupar a CPU!
			if( !LISTA_BCP_vazia(prontos) )
				executando = experimento->politica->escalonar(experimento->politica);
            if(executando != NULL){
                trocas_de_contexto++;
                
				//contabiliza o tempo de espera desse processo ao TME
				tme += relogio - executando->tExecRecente;
				
				// marca esse tempo de execucao como o mais recente
				executando->tExecRecente = relogio;

                if(executando->tPrimeiraExec == -1){
                    executando->tPrimeiraExec = relogio+1;
                }
                
            }
            else{
                tempo_ocioso++;
            }
        }        

        relogio++;
    }
    
	// calculo do tme, tmr e vazao
	tme = tme / processos->nProcessos;
	vazao = qtdProcExecutados /(relogio/TEMPO_VAZAO);
    
	// escreve os dados do cabecalho;
	// grava o cabecalho no log
	char *cabecalho = malloc(BUFFER_HEADER * sizeof(char));
    sprintf(cabecalho, "Chaveamentos: %" PRIu64, trocas_de_contexto);
	recordEvent(logger, cabecalho, HEADER);

    sprintf(cabecalho, "TME: %Lf", tme);
	recordEvent(logger, cabecalho, HEADER);

    sprintf(cabecalho, "TMR: %Lf", tmr);
	recordEvent(logger, cabecalho, HEADER);

    sprintf(cabecalho, "Vazao: %Lf", vazao);
	recordEvent(logger, cabecalho, HEADER);

    sprintf(cabecalho, "Tempo ocioso: %" PRIu64, tempo_ocioso);
	recordEvent(logger, cabecalho, HEADER);

	// imprime o log de evento
	getLog(logger, NULL);

#ifdef DEBUG
	printf("DEBUG: Relogio: %" PRIu64 "\n", relogio);
#endif
    
	// libera a memoria da string de termino dos processos
	free(cabecalho);
	free(logger);

    return (EXIT_SUCCESS);
}

