#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

////////////////////////////////////////////////
//  PRACTICA FINAL SISTEMAS OPERATIVOS 2020   //    
//     - Laura Daniela Cossio Pulido          //
//     - Maria Gil Getino                     //
//     - Esther Mateos Cachan                 // 
//     - Sara Real de la Fuente               //
////////////////////////////////////////////////

//constantes 
#define CAPACIDAD_CONSULTORIO 15 //solo puede haber 15 personas en el consultorio

//semaforos
pthread_mutex_t mutexAccesoLog;
pthread_mutex_t mutexAccesoColaPacientes;
pthread_mutex_t mutexEstadistico;
pthread_mutex_t mutexVacunacion;
pthread_mutex_t mutexReaccion;

pthread_cond_t condEstadistico;
pthread_cond_t condVacunacion;
pthread_cond_t condReaccion;

//variables glogales
int contadorPacientes; //pacientes que llegan a la cola
int enfermeros; //son 3
int medico; //es 1
int estadistico;//es 1
int finalizar; //para que termine el programa
int pacientesEnEstudio;
int contadorPacientesTotal; //pacientes que llegan al consultorio

//int colaPacientes[CAPACIDAD_CONSULTORIO]; 

FILE *logFile;//Fichero del log
char *logFileName = "log.log";

//estructuras  
//estructuras  
struct paciente{
    int id;
    int atendido;
    int tipo;
    int serologia;
    pthread_t hiloEjecucionPaciente;
};

struct paciente *listaPacientes; //lista15pacientes


void inicializaSemaforos(); 
void nuevoPaciente(int sig);
void *accionesPaciente(void *id);
void *accionesEnfermero(void *id);
void *accionesMedico();
void *accionesEstadistico();
void finPrograma();

int obtienePosPaciente(int identificador);
void atenderPacientes(int id, int posicion);
void writeLogMessage(char *id, char *msg);
void iniciarLog(); 
void sacaPacienteDeCola(int posicion);
int pacientesEnCola(int tipoPaciente);
int cogerPaciente(int identificadorEnfermero);
int cogerPacienteExtra();
int cogerPacienteReaccion();
int calculaAleatorios(int min, int max);


int main(){

    srand(getpid());
    int i;
    int enfermero1=0;
    int enfermero2=1;
    int enfermero3=2;
    contadorPacientes = 0;
    finalizar = 0;
    enfermeros = 0;
    medico = 0;
    estadistico = 0;
    contadorPacientesTotal=0;
    pacientesEnEstudio = 0;

    listaPacientes = (struct paciente*)malloc(sizeof(struct paciente)*CAPACIDAD_CONSULTORIO);
    for(i=0;i<CAPACIDAD_CONSULTORIO;i++)
    {
        listaPacientes[i].id = -1;
        listaPacientes[i].atendido = 0;
        listaPacientes[i].tipo = 0;
        listaPacientes[i].serologia = 0;
    }
    inicializaSemaforos();
    //iniciarLog();

    

    //SIGUSR1 pacientes junior 
    if(signal(SIGUSR1, nuevoPaciente)==SIG_ERR){
        exit(-1);
    }  
    //SIGUSR2 pacientes medios
    if(signal(SIGUSR2, nuevoPaciente)==SIG_ERR){
        exit(-1);
    }
    //SIGPIPE pacientes senior
    if(signal(SIGPIPE, nuevoPaciente)==SIG_ERR){
        exit(-1);
    }
    //SIGINT finaliza el programa
    if(signal(SIGINT, finPrograma)==SIG_ERR){
        exit(-1);
    }
    
    pthread_t enfermeroHilo_1, enfermeroHilo_2, enfermeroHilo_3, medicoHilo, estadisticoHilo;
    pthread_create(&enfermeroHilo_1, NULL, accionesEnfermero, &enfermero1);
    pthread_create(&enfermeroHilo_2, NULL, accionesEnfermero, &enfermero2);
    pthread_create(&enfermeroHilo_3, NULL, accionesEnfermero, &enfermero3);

    //Crear el hilo medico:
    pthread_create(&medicoHilo, NULL, accionesMedico, NULL);

    //crear el hilo estadistico
    pthread_create(&estadisticoHilo, NULL, accionesEstadistico, NULL);

    //esperar por senales de forma infinita:
    do{
        pause();

    }while(finalizar == 0);
    

    free(listaPacientes);

    pthread_exit(NULL);
    return 0;
}

void inicializaSemaforos()
{
    if(pthread_mutex_init (&mutexAccesoLog, NULL)!=0) exit(-1);
    if(pthread_mutex_init (&mutexAccesoColaPacientes, NULL)!=0) exit(-1);
    if( pthread_mutex_init (&mutexEstadistico, NULL)!=0) exit(-1);
    if(pthread_mutex_init (&mutexVacunacion, NULL)!=0) exit(-1);
    if(pthread_mutex_init (&mutexReaccion, NULL)!=0) exit(-1);

    if(pthread_cond_init(&condEstadistico, NULL)!=0) exit(-1);
    if(pthread_cond_init(&condVacunacion, NULL)!=0) exit(-1);
    if(pthread_cond_init(&condReaccion, NULL)!=0) exit(-1);
}

void nuevoPaciente (int sig){
    char logInicio[100]; 

    signal(SIGUSR1, nuevoPaciente);
    signal(SIGUSR2, nuevoPaciente);
    signal(SIGPIPE, nuevoPaciente);


    if ( 1){    //Condicion para acabar
        if(contadorPacientes < CAPACIDAD_CONSULTORIO){
            int pos = 0;
            int i =0;
            int stop = 0;

            pthread_mutex_lock(&mutexAccesoColaPacientes);

            for(i = 0; i < CAPACIDAD_CONSULTORIO && stop == 0; i++){
                if(listaPacientes[i].id == -1) {
                    pos = i;
                    stop = 1;
                }
            }

            int identif = contadorPacientesTotal;
            listaPacientes[pos].id = identif;
            listaPacientes[pos].atendido = 0;
            listaPacientes[pos].serologia = 0;
            

            if(sig == SIGUSR1){
            listaPacientes[pos].tipo = 0;
            sprintf(logInicio, "Nuevo paciente, tipo 0 ");
            writeLogMessage(logInicio, "");
            }
            if(sig == SIGUSR2){
                listaPacientes[pos].tipo = 1;
                printf("Nuevo paciente, tipo 1\n");
                sprintf(logInicio, "Nuevo paciente, tipo 1");
                writeLogMessage(logInicio, "");
            }
            if(sig == SIGPIPE){
                listaPacientes[pos].tipo = 2;
                printf("Nuevo paciente, tipo 2\n");
                sprintf(logInicio, "Nuevo paciente, tipo 2");
                writeLogMessage(logInicio, "");
            }
            contadorPacientesTotal++;
            contadorPacientes++;
        pthread_mutex_unlock(&mutexAccesoColaPacientes);
        pthread_create(&listaPacientes[pos].hiloEjecucionPaciente, NULL, accionesPaciente, (void *)&identif);

        
         //pthread_create(&enfermoHilo1, NULL, accionesPaciente, (void *)&identif);
       
            
        }else{ //No hay hueco
            char logSalidaCreacionPaciente[100];
             printf("No hay espacio para más pacientes en la lista");
        }
    }  
}

void *accionesPaciente(void *id){
    int aux = 0;
    char logPacientes[100];
    char logIdentificador[50];
   
    int comportamientoPaciente;
    int reaccionPaciente;
    int catarroPaciente;
    int auxSalida = 1;
    int identificador = *(int*)id;

    sprintf(logIdentificador, "[Paciente]");


    pthread_mutex_lock(&mutexAccesoColaPacientes);
        int posicion = obtienePosPaciente(identificador);

        /*Guarda en el log la hora de entrada*/
        sprintf(logPacientes, "%d ha entrado en la posicion %d", identificador, posicion);
        writeLogMessage(logIdentificador, logPacientes);
        
        int tipo = listaPacientes[posicion].tipo;
    pthread_mutex_unlock(&mutexAccesoColaPacientes);
    
    /*Guarda en el log el tipo de solicitud*/
    sprintf(logPacientes, "%d es de tipo %d",identificador, tipo);
    writeLogMessage(logIdentificador, logPacientes);

    /*Duerme 3 segundos*/
    sleep(3);
   
   /* Comprueba si esta siendo atendido */     
    while(listaPacientes[posicion].atendido == 0){

        /*Calculamos el comportamiento del paciente*/

        /*Duerme 3 segundos*/
        sleep(3);
    }
    sprintf(logPacientes, "%d esta siendo atendido", identificador);
    writeLogMessage(logIdentificador, logPacientes);

    /*Esperar a que termine de ser atendido*/
    while(listaPacientes[posicion].atendido != 2){
       sleep(1); 
    }

    /*Calculamos si ha tenido reaccion*/
    reaccionPaciente = calculaAleatorios(0,99);
    
    if(reaccionPaciente < 10){ //Ha tenido reaccion

        /* Si le da ponemos el valor de atendido a 4*/
        pthread_mutex_lock(&mutexAccesoColaPacientes);
            listaPacientes[posicion].atendido = 4;
        pthread_mutex_unlock(&mutexAccesoColaPacientes);

        sprintf(logPacientes, "%d ha tenido reaccion. %d", identificador, reaccionPaciente);
        writeLogMessage(logIdentificador, logPacientes);

        /*Esperamos a que termine su atencion con el medico*/
        while(listaPacientes[posicion].atendido != 5){
            sleep(1);
        }
    }else{ //Puede hacer el estudio serologico
        reaccionPaciente = calculaAleatorios(0,99);

        if(reaccionPaciente < 25){ 
            pthread_mutex_lock(&mutexAccesoColaPacientes);
                listaPacientes[posicion].serologia = 1;
            pthread_mutex_unlock(&mutexAccesoColaPacientes);

            sprintf(logPacientes, "%d realizará el estudio serologico", identificador);
            writeLogMessage(logIdentificador, logPacientes);

            //Espera a que el estadistico termine
            pthread_mutex_lock(&mutexEstadistico);
                pacientesEnEstudio = 1;
            pthread_cond_wait(&condEstadistico, &mutexEstadistico);

            pthread_mutex_unlock(&mutexEstadistico);

            sprintf(logPacientes, "%d sale del estudio serologico", identificador);
            writeLogMessage(logIdentificador, logPacientes);
        }
    }
    //Cerramos el hilo
    sprintf(logPacientes, "%d acaba y sale del consultorio.", identificador);
    writeLogMessage(logIdentificador, logPacientes);

    //Liberamos su espacio en cola
    sacaPacienteDeCola(posicion);
   
}

void *accionesEnfermero(void *id){
    int i=0, encontrado=0, contadorCafe=0, posicion=0;
    char logEnfermero[100];
    char logIdentificador[50];

    int tipoPaciente=0;
    int ocupado = 0;
    int comprueboOtraVez = -1;
    int atiendoA = -1;
    int identificador = *(int*)id; //El identificador del enfermero
    
   
    sprintf(logIdentificador, "[Enfermero %d]", identificador);
    
    while(1){
           
        /*Busco si hay pacientes, sino duermo*/
        while(pacientesEnCola(0) == 0){
            sleep(1);
        }
        do{
            /*Busco de mi tipo*/
            atiendoA = cogerPaciente(identificador);

            /*Si no hay busco de otro y vuelvo a comprobar que no haya de mi tipo*/
            if (atiendoA == -1) {
                atiendoA = cogerPacienteExtra();
                comprueboOtraVez = cogerPaciente(identificador); // Para evitar que se roben
                if (comprueboOtraVez != -1) {
                    atiendoA = comprueboOtraVez;
                }
            }
            //Sleep?
        }
        while(atiendoA == -1);

        /*Cambiamos el flag de atendido y buscamos la posicion del paciente*/
        pthread_mutex_lock(&mutexAccesoColaPacientes);
         
            posicion = obtienePosPaciente(atiendoA);
            listaPacientes[posicion].atendido=1;

        pthread_mutex_unlock(&mutexAccesoColaPacientes);

        sprintf(logEnfermero, "Va a atender a %d, en la posicion %d", atiendoA, posicion);
        writeLogMessage(logIdentificador, logEnfermero);

        /*Atiende al paciente*/
        atenderPacientes(atiendoA,posicion);
        
        /*Cambiamos el flag de atendido*/
        pthread_mutex_lock(&mutexAccesoColaPacientes);
            listaPacientes[posicion].atendido=2; //ya ha sido atendido
        pthread_mutex_unlock(&mutexAccesoColaPacientes);

        /*Comprueba si toma cafe*/
        //TODO

    }
       
}

void *accionesMedico(){
    int i=0, encontrado=0, contadorCafe=0, posicion=0;
    int tipoPaciente=0;
    int atiendoA = -1;
    int atiendoAReaccion = -1;

    char logMedico[100];
    char logIdentificador[50];

    sprintf(logIdentificador, "[Medico]");

    while(1){
        /*Busca pacientes que no hayan sido atendidos o que tengan reaccion*/
        while(pacientesEnCola(0) == 0 && pacientesEnCola(4) == 0){
            sleep(1);
        }
        /*Busca primero si hay alguno con reaccion, luego si hay alguno no atendido*/
        do{
            atiendoAReaccion = cogerPacienteReaccion();
            if (atiendoAReaccion == -1) {
                atiendoA = cogerPacienteExtra();
            }
            sleep(1);
        }
        while(atiendoA == -1 && atiendoAReaccion == -1);
        /*Esta en el bucle mientras no existan ambos tipos*/

        /*Si tiene reaccion atendemos primero*/
        if(atiendoAReaccion != -1){ 
            pthread_mutex_lock(&mutexAccesoColaPacientes);
                posicion = obtienePosPaciente(atiendoAReaccion); 
            pthread_mutex_unlock(&mutexAccesoColaPacientes);
            sleep(5);
            /*Cambiamos el flag de atendido*/
            pthread_mutex_lock(&mutexAccesoColaPacientes);
               listaPacientes[posicion].atendido=5; 
            pthread_mutex_unlock(&mutexAccesoColaPacientes);

            sprintf(logMedico, "Atendido paciente %d con reaccion.", atiendoAReaccion);
            writeLogMessage(logIdentificador, logMedico);
        
        }
        /*Si no hay con reaccion, puedo vacunar a un paciente*/
        else if(atiendoA != -1){ 
            pthread_mutex_lock(&mutexAccesoColaPacientes);
                posicion = obtienePosPaciente(atiendoA);
                if(listaPacientes[posicion].atendido==0){
                    listaPacientes[posicion].atendido=1;
                    pthread_mutex_unlock(&mutexAccesoColaPacientes);

                    sprintf(logMedico,"Va a atender a %d", atiendoA);
                    writeLogMessage(logIdentificador, logMedico);
                    
                    atenderPacientes(atiendoA, posicion);

                    sprintf(logMedico,"Termina de atender a %d", atiendoA);
                    writeLogMessage(logIdentificador, logMedico);

                    pthread_mutex_lock(&mutexAccesoColaPacientes);
                        listaPacientes[posicion].atendido=2; //ya ha sido atendido
                        int aux = listaPacientes[posicion].atendido;
                    pthread_mutex_unlock(&mutexAccesoColaPacientes);

                    sprintf(logMedico,"Envio la señal de que termine de vacunar a %d", posicion);
                    writeLogMessage(logIdentificador, logMedico);
                }
            pthread_mutex_unlock(&mutexAccesoColaPacientes);
        }
     }

}

void *accionesEstadistico(){
    int posicion = 0;
    int i = 0;
    int salir = 0;
    char losEstadistico[100];
    char losEstadistico2[100];

    while(1) {

        //Espera que le avisen de que hay paciente en estudio
        do {
            sleep(1);
        } while (pacientesEnEstudio == 0);
            
        // Buscamos quien es
        pthread_mutex_lock(&mutexAccesoColaPacientes);
            for(i=0; i< CAPACIDAD_CONSULTORIO && salir == 0; i++){
                if(listaPacientes[i].serologia = 1){
                    posicion = i;
                    salir = 0;
                }
            }
        pthread_mutex_unlock(&mutexAccesoColaPacientes);

        //Escribe log que comienza actividad
        sprintf(losEstadistico, "[Estadístico] Un paciente va a iniciar el estudio serológico");
        sprintf(losEstadistico2, "...realizando encuesta y tomando los datos del paciente...");
        writeLogMessage(losEstadistico, losEstadistico2);

        //Calcula tiempo de actividad(4segundos)
        sleep(4);

        //Termina la actividad y avisa al paciente
        pthread_mutex_lock(&mutexEstadistico);
            pthread_cond_signal(&condEstadistico); // Avisa al paciente para que se vaya
            pacientesEnEstudio = 0;
        pthread_mutex_unlock(&mutexEstadistico);

        //Escribe log finaliza actividad
        sprintf(losEstadistico, "[Estadístico] El paciente ha terminado el estudio serológico");
        writeLogMessage(losEstadistico, "");

        //Cambia paciente en estudio y vuelve a 1
        pthread_mutex_lock(&mutexAccesoColaPacientes);

            listaPacientes[posicion].serologia = 2;

        pthread_mutex_unlock(&mutexAccesoColaPacientes);
    }
}

void atenderPacientes(int id, int posicion){
    int tipoAtencion = 0;
    int tiempoGripe = 0, tiempoMalDocumentado = 0, tiempoBien = 0;

    char atenderLog[100];
    char logIdentificador[50];

    tipoAtencion = calculaAleatorios(0,99);
    tiempoGripe = calculaAleatorios(6,10);
    tiempoMalDocumentado = calculaAleatorios(2,6);
    tiempoBien = calculaAleatorios(1,4);

    sprintf(logIdentificador, "[Enfermero/Medico]");

    /*Tiene catarro o gripe*/
    if(tipoAtencion < 10){ //Tiene catarro o gripe

        sprintf(atenderLog, "Paciente %d tiene gripe por tanto NO se vacuna", id);
        writeLogMessage(logIdentificador, atenderLog);
        sleep(tiempoGripe);
        sprintf(atenderLog, "Paciente %d sale del consultorio con gripe", id);
        writeLogMessage(logIdentificador, atenderLog);

        sacaPacienteDeCola(posicion);

    }
    /*Esta mas identificado*/
    else if(tipoAtencion < 20){
        sprintf(atenderLog, "Paciente %d Esta mal identificado, comienza la atencion al paciente", id);
        writeLogMessage(logIdentificador, atenderLog);
        sleep(tiempoMalDocumentado);
        sprintf(atenderLog, "Paciente %d Esta mal identificado, finaliza la atencion al paciente", id);
        writeLogMessage(logIdentificador, atenderLog);
    }
    /*Todo esta correcto*/
    else{ 
        sprintf(atenderLog, "Paciente %d tiene todo en regla, comienza la atencion al paciente", id);
        writeLogMessage(logIdentificador, atenderLog);
        sleep(tiempoBien);
        sprintf(atenderLog, "Paciente %d tiene todo en regla, finaliza la atencion al paciente", id);
        writeLogMessage(logIdentificador, atenderLog);

    }

}

int pacientesEnCola(int tipoPaciente){
    int i = 0, j = 0;

    pthread_mutex_lock(&mutexAccesoColaPacientes);

        for(i = 0; i < CAPACIDAD_CONSULTORIO; i++){
            if(listaPacientes[i].id != -1 && listaPacientes[i].atendido == tipoPaciente){
                j++;
            }
        }    

    pthread_mutex_unlock(&mutexAccesoColaPacientes);
    return j;
}

int cogerPaciente(int identificadorEnfermero){
    int i = 0; 
    int id = -1;
    int salirFor = 0;
    pthread_mutex_lock(&mutexAccesoColaPacientes);
        for(i = 0; i < CAPACIDAD_CONSULTORIO && salirFor == 0; i++){
            if(listaPacientes[i].atendido == 0 && listaPacientes[i].tipo == identificadorEnfermero && listaPacientes[i].id != -1){
                id = listaPacientes[i].id;
                salirFor = 1;
            }
        }
    pthread_mutex_unlock(&mutexAccesoColaPacientes);
    return id;

}

int cogerPacienteExtra(){ //Si quiero que los recorra en orden, un break en el segundo for
    int i = 0; 
    int id = -1;
    int count0 = 0, count1 = 0, count2 = 0;
    int tipoPaciente;
    int tipoQueMasTiene;

    pthread_mutex_lock(&mutexAccesoColaPacientes);
        for(i = 0; i < CAPACIDAD_CONSULTORIO; i++){
            if(listaPacientes[i].atendido == 0 && listaPacientes[i].id != -1){
                tipoPaciente = listaPacientes[i].tipo;

                if(tipoPaciente == 0) count0++;
                else if(tipoPaciente == 1) count1++;
                else if(tipoPaciente == 2) count2++;
                
            }
        }
        //Calculo el maximo de ellos
        tipoQueMasTiene = 0;
        if(count0 > count1 && count0 > count2) tipoQueMasTiene = 0;
        else if(count1 > count0 && count1 > count2) tipoQueMasTiene = 1;
        else if(count2 > count0 && count2 > count1) tipoQueMasTiene = 2;
        //pthread_mutex_unlock(&mutexAccesoColaPacientes);
        
        for(i = 0; i < CAPACIDAD_CONSULTORIO; i++){
           
            if(listaPacientes[i].atendido == 0 && listaPacientes[i].tipo == tipoQueMasTiene && listaPacientes[i].id != -1){
                id = listaPacientes[i].id;
                //printf("paso por aqui\n");
            }
        }
    pthread_mutex_unlock(&mutexAccesoColaPacientes);
    // printf("El medico atenderia a; %d\n", id);
    return id;

}
int cogerPacienteReaccion(){
    int i = 0; 
    int id = -1;
    int salirFor = 0;
    pthread_mutex_lock(&mutexAccesoColaPacientes);
        for(i = 0; i < CAPACIDAD_CONSULTORIO && salirFor == 0; i++){
            if(listaPacientes[i].atendido == 4 && listaPacientes[i].id != -1){
                id = listaPacientes[i].id;
                salirFor = 1;
            }
        }
    pthread_mutex_unlock(&mutexAccesoColaPacientes);
    // printf("El medico atiende con reaccion a; %d\n", id);
    return id;

}

int obtienePosPaciente(int identificador){
    int posicion = -1;
    int i = 0;
    char msj[100];
    
    
    //Obtiene la posicion del paciente en la cola de pacientes
    for(i = 0; i < CAPACIDAD_CONSULTORIO; i++){
        if(listaPacientes[i].id == identificador){
            posicion = i;
        }
    }
    return posicion;
}
void sacaPacienteDeCola(int posicion){

    pthread_mutex_lock(&mutexAccesoColaPacientes);
    //Sacamos el paciente de la cola

    listaPacientes[posicion].id = -1;
    listaPacientes[posicion].atendido = 0;
    listaPacientes[posicion].tipo = 0;
    listaPacientes[posicion].serologia = 0;
    contadorPacientes--;

    pthread_mutex_unlock(&mutexAccesoColaPacientes); 
    pthread_exit(NULL);
}
void finPrograma(){
    
    int i=0;
    int salir = 0;
    
    pthread_mutex_lock(&mutexAccesoColaPacientes);
    
    while(i<contadorPacientes && salir == 0){
        salir = 1;
        
        if(listaPacientes[i].atendido == 0){//no ha sido atendido todavia
                salir = 0;
                //sleep(1);
            }
            i++;
        }
  
    pthread_mutex_unlock(&mutexAccesoColaPacientes);

    char finVacunaciones[100];
    printf("La vacunación ha terminado");

    //salimos del porgrama
    finalizar=1;
}

void iniciarLog(){
    logFile=fopen(logFileName,"w");
    fclose(logFile);
    writeLogMessage("Consultorio Vacunacion Covid-19", "");
}

void writeLogMessage(char *id, char *msg){
    pthread_mutex_lock (&mutexAccesoLog);
    time_t now =time(0);
    struct tm *tlocal = localtime(&now);
    char stnow[25];
    strftime(stnow,25, "%d/%m/ %y %H:%M:%S", tlocal);
    logFile = fopen(logFileName, "a");
    fprintf(logFile,"[%s] %s: %s\n", stnow, id,msg);
    fclose(logFile);
    pthread_mutex_unlock (&mutexAccesoLog);
}
int calculaAleatorios(int min, int max) {
    return rand() % (max-min+1) + min;
}