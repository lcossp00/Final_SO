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
pthread_mutex_t mutexPacientesEnEstudio;

pthread_cond_t condEstadistico;

//variables glogales
int contadorPacientes; //pacientes que llegan a la cola
int enfermeros; //son 3
int medico; //es 1
int estadistico;//es 1
int finalizar; //para que termine el programa
int pacientesEnEstudio; // Para saber si el estadistico tiene un paciente
int contadorPacientesTotal; //pacientes que llegan al consultorio

FILE *logFile;//Fichero del log
char *logFileName = "registroTiempos.log";

//Estructuras   
struct paciente{
    int id;
    int atendido;
    int tipo;
    int serologia;
    pthread_t hiloEjecucionPaciente;
};

struct paciente *listaPacientes; //lista15pacientes


void inicializaSemaforos(); //Incializa los semaforos
void nuevoPaciente(int sig); //Manejadora de los hilos Paciente
void *accionesPaciente(void *id); //Funcion para los hilos Paciente
void *accionesEnfermero(void *id); //Funcion para los hilos Enfermero
void *accionesMedico(); //Funcion para el hilo Medico
void *accionesEstadistico(); //Funcion para el hilo Estadistico
void finPrograma(); //Funcion que termina el programa

int obtienePosPaciente(int identificador); //Devuelve la posicion en lista que tiene un paciente
void atenderPacientes(int id, int posicion, int identificadorProfesional); //Vacuna y atiende a un paciente
void sacaPacienteDeCola(int posicion); //Saca el paciende indicado de la cola
int pacientesEnCola(int tipoAtendido); //Devuelve el numero de pacientes en cola con el valor atendido indicado como argumento
int cogerPaciente(int identificadorEnfermero); //Uso: Enfermero. Buscar el siguiente paciente a atender segun tipo enfermero
int cogerPacienteExtra(); //Uso: Enfermero y Medico. Busca un paciente extra
int cogerPacienteReaccion(); //Uso: Medico. Coge el primer paciente con reaccion
int pacientesRestantes(); //Uso: FinalizarPrograma. devuelve la variable contadorPacientes, protegida con mutex
int calculaAleatorios(int min, int max);
void writeLogMessage(char *id, char *msg); 
void iniciarLog(); 



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
    iniciarLog();

    

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
    if( pthread_mutex_init (&mutexPacientesEnEstudio, NULL)!=0) exit(-1);

    if(pthread_cond_init(&condEstadistico, NULL)!=0) exit(-1);
}

void nuevoPaciente (int sig){
    char logInicio[100]; 

    signal(SIGUSR1, nuevoPaciente);
    signal(SIGUSR2, nuevoPaciente);
    signal(SIGPIPE, nuevoPaciente);

    if(contadorPacientes < CAPACIDAD_CONSULTORIO){
        int pos = 0;
        int i = 0;
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
        sprintf(logInicio, "Nuevo paciente, tipo");
        writeLogMessage(logInicio, "0");
        }
        if(sig == SIGUSR2){
            listaPacientes[pos].tipo = 1;
            printf("Nuevo paciente, tipo 1\n");
            sprintf(logInicio, "Nuevo paciente, tipo");
            writeLogMessage(logInicio, "1");
        }
        if(sig == SIGPIPE){
            listaPacientes[pos].tipo = 2;
            printf("Nuevo paciente, tipo 2\n");
            sprintf(logInicio, "Nuevo paciente, tipo");
            writeLogMessage(logInicio, "2");
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

void *accionesPaciente(void *id){
    char logPacientes[100];
    char logIdentificador[50];
   
    int reaccionPaciente;
    int catarroPaciente;
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

    int aux1 = 0;
   
   /* Comprueba si esta siendo atendido */     
    while(listaPacientes[posicion].atendido == 0){
        if(aux1 == 1){
            /*Calculamos el comportamiento del paciente*/
            reaccionPaciente = calculaAleatorios(0,99);

            if(reaccionPaciente  < 10){
                sprintf(logPacientes, "El paciente %d sale del consultorio. Se lo ha pensado mejor", identificador);
                writeLogMessage(logIdentificador, logPacientes);
                sacaPacienteDeCola(posicion);
            }else if(reaccionPaciente < 30){
                sprintf(logPacientes, "El paciente %d sale del consultorio. Se ha cansado de esperar", identificador);
                writeLogMessage(logIdentificador, logPacientes);
                sacaPacienteDeCola(posicion);
            }else if(reaccionPaciente < 35){
                sprintf(logPacientes, "El paciente %d sale del consultorio. Fue al baño y perdio su turno", identificador);
                writeLogMessage(logIdentificador, logPacientes);
                sacaPacienteDeCola(posicion);

            }
        }
        aux1 = 1;
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

        sprintf(logPacientes, "%d ha tenido reaccion.", identificador);
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

            //En la variable pacientesEnEstudio pongo que hay un paciente listo
            pthread_mutex_lock(&mutexPacientesEnEstudio);
        
                pacientesEnEstudio = 1;

            pthread_mutex_unlock(&mutexPacientesEnEstudio);

            //Espera a que el estadistico termine
            pthread_mutex_lock(&mutexEstadistico);
            
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
        // Ahora empieza a haber pacientes
        do{
            /*Busco de mi tipo*/
            atiendoA = cogerPaciente(identificador);

            /*Si no hay busco de otro y vuelvo a comprobar que no haya de mi tipo*/
            if (atiendoA == -1) {
                atiendoA = cogerPacienteExtra();
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
        atenderPacientes(atiendoA,posicion,identificador);
        
        /*Cambiamos el flag de atendido*/
        pthread_mutex_lock(&mutexAccesoColaPacientes);
            listaPacientes[posicion].atendido=2; //ya ha sido atendido
        pthread_mutex_unlock(&mutexAccesoColaPacientes);

        /*Comprueba si toma cafe*/
        contadorCafe++;
        /*Cada vez que se atienden 5 pacientes, el enfermer@ descansa 5 segundos*/
        if(contadorCafe==5)
        {
            sleep(5);
            contadorCafe=0;
            sprintf(logEnfermero, "Va a tomar un cafe");
            writeLogMessage(logIdentificador, logEnfermero);
        }

    }

    pthread_exit(NULL);
       
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
                    
                    atenderPacientes(atiendoA, posicion, 3);//3 es la forma de indicar que es medico en atender paciente 

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

     pthread_exit(NULL);

}

void *accionesEstadistico(){
    int posicion = 0;
    int i = 0;
    int salir = 0;
    char losEstadistico[100];
    char losEstadistico2[100];
    int enEstudio = 0;

    while(1) {
        //Espera que le avisen de que hay paciente en estudio
        do {
            pthread_mutex_lock(&mutexPacientesEnEstudio);
                enEstudio = pacientesEnEstudio;
            pthread_mutex_unlock(&mutexPacientesEnEstudio);

            sleep(3);
        } while (enEstudio == 0);
            
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
        pthread_mutex_unlock(&mutexEstadistico);

        pthread_mutex_lock(&mutexPacientesEnEstudio);  
            pacientesEnEstudio = 0;
        pthread_mutex_unlock(&mutexPacientesEnEstudio);

        //Escribe log finaliza actividad
        sprintf(losEstadistico, "[Estadístico] El paciente ha terminado el estudio serológico");
        writeLogMessage(losEstadistico, "");

        //Cambia paciente en estudio y vuelve a 1
        pthread_mutex_lock(&mutexAccesoColaPacientes);

            listaPacientes[posicion].serologia = 2;

        pthread_mutex_unlock(&mutexAccesoColaPacientes);
    }
}

void atenderPacientes(int id, int posicion, int identificadorProfesional){
    int tipoAtencion = 0;
    int tiempoGripe = 0, tiempoMalDocumentado = 0, tiempoBien = 0;

    char atenderLog[100];
    char logIdentificador[50];

    tipoAtencion = calculaAleatorios(0,99);
    tiempoGripe = calculaAleatorios(6,10);
    tiempoMalDocumentado = calculaAleatorios(2,6);
    tiempoBien = calculaAleatorios(1,4);


    // Si la id profesional es 3, el identificador es medico, sino, es el identificador del enfermero.
    if(identificadorProfesional == 3) sprintf(logIdentificador, "[Medico]");
    else sprintf(logIdentificador, "[Enfermero %d]",identificadorProfesional);

    /*Tiene catarro o gripe*/
    if(tipoAtencion < 10){ 

        sprintf(atenderLog, "Paciente %d tiene gripe por tanto NO se vacuna", id);
        writeLogMessage(logIdentificador, atenderLog);
        sleep(tiempoGripe);
        sprintf(atenderLog, "Paciente %d sale del consultorio con gripe", id);
        writeLogMessage(logIdentificador, atenderLog);

        sacaPacienteDeCola(posicion);

    }
    else if(tipoAtencion < 20){     /*Esta mas identificado*/
        sprintf(atenderLog, "Paciente %d Esta mal identificado, comienza la atencion al paciente", id);
        writeLogMessage(logIdentificador, atenderLog);
        sleep(tiempoMalDocumentado);
        sprintf(atenderLog, "Paciente %d Esta mal identificado, finaliza la atencion al paciente", id);
        writeLogMessage(logIdentificador, atenderLog);
    }
    else{                           /*Todo esta correcto*/
        sprintf(atenderLog, "Paciente %d tiene todo en regla, comienza la atencion al paciente", id);
        writeLogMessage(logIdentificador, atenderLog);
        sleep(tiempoBien);
        sprintf(atenderLog, "Paciente %d tiene todo en regla, finaliza la atencion al paciente", id);
        writeLogMessage(logIdentificador, atenderLog);

    }

}

int pacientesEnCola(int tipoAtendido){
    int i = 0, j = 0;

    pthread_mutex_lock(&mutexAccesoColaPacientes);

        for(i = 0; i < CAPACIDAD_CONSULTORIO; i++){
            if(listaPacientes[i].id != -1 && listaPacientes[i].atendido == tipoAtendido){
                j++;
            }
        }    

    pthread_mutex_unlock(&mutexAccesoColaPacientes);
    return j;
}

int cogerPaciente(int identificadorEnfermero){
    int i = 0; 
    int id = -1;

    pthread_mutex_lock(&mutexAccesoColaPacientes);
    int min = contadorPacientesTotal;
        for(i = 0; i < CAPACIDAD_CONSULTORIO; i++){
            if(listaPacientes[i].atendido == 0 && listaPacientes[i].tipo == identificadorEnfermero && listaPacientes[i].id != -1){
               if(listaPacientes[i].id < min && listaPacientes[i].id != -1){
                    min = listaPacientes[i].id;
                    id = min;
                }
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
    int tipoQueMasTiene[2]; // En [0] guardamos cual es el tipo, en [1] guardamos cuantos

    pthread_mutex_lock(&mutexAccesoColaPacientes);
        for(i = 0; i < CAPACIDAD_CONSULTORIO; i++){
            if(listaPacientes[i].atendido == 0 && listaPacientes[i].id != -1){
                tipoPaciente = listaPacientes[i].tipo;

                if(tipoPaciente == 0) count0++;
                else if(tipoPaciente == 1) count1++;
                else if(tipoPaciente == 2) count2++;
                
            }
        }
    pthread_mutex_unlock(&mutexAccesoColaPacientes);
        //Calculo el maximo de ellos
        tipoQueMasTiene[0] = 0;
        tipoQueMasTiene[1] = 0;
        if(count0 > count1 && count0 > count2) {
            tipoQueMasTiene[0] = 0;
            tipoQueMasTiene[1] = count0;
        } else if(count1 > count0 && count1 > count2) {
            tipoQueMasTiene[0] = 1;
            tipoQueMasTiene[1] = count1;
        } else if(count2 > count0 && count2 > count1) {
            tipoQueMasTiene[0] = 2;
            tipoQueMasTiene[1] = count2;
        }
        //pthread_mutex_unlock(&mutexAccesoColaPacientes);

    pthread_mutex_lock(&mutexAccesoColaPacientes);
        
        for(i = 0; i < CAPACIDAD_CONSULTORIO; i++){
           
            if(listaPacientes[i].atendido == 0 && listaPacientes[i].tipo == tipoQueMasTiene[0] && listaPacientes[i].id != -1){
                if (tipoQueMasTiene[1] > 1) // SOLO cogemos otro si le sobran, o sea, si tiene más de 1
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

    pthread_cancel(listaPacientes[posicion].hiloEjecucionPaciente);
    pthread_mutex_unlock(&mutexAccesoColaPacientes); 
    
}

int pacientesRestantes() {
    pthread_mutex_lock(&mutexAccesoColaPacientes);
    finalizar=1;
    int contador = contadorPacientes;
    pthread_mutex_unlock(&mutexAccesoColaPacientes);
    return contador;

}

void finPrograma(){
    signal(SIGINT, finPrograma);

    char finalizarLog[100];
    char identificadorLog[50];
    int i=0;
    int salir = 0;

    sprintf(identificadorLog,"Finalizar Programa");
    sprintf(finalizarLog,"Se envia la señal de terminar Vacunacion");
    writeLogMessage(identificadorLog, finalizarLog);
     
    while(pacientesRestantes() != 0){
        sleep(1);
    }

    sprintf(finalizarLog,"La vacunación ha terminado");
    writeLogMessage(identificadorLog, finalizarLog);

    //salimos del porgrama
    exit(0);
}

void iniciarLog(){
    logFile=fopen(logFileName,"w");
    fclose(logFile);
    writeLogMessage("\nBienvenido al Consultorio Vacunacion Covid-19", "\n");
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