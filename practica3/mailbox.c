#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>

#include "sem.h"
#include "mailbox.h"
#include "cbuffer.h"

#define UMAX(a, b)      ((a) > (b) ? (a) : (b))



/* El buzón en sí se crea mediante una llamada a mbox_new, que realiza el programa principal.
 * Como el buzón está en el "heap" es COMPARTIDO POR TODOS LOS HILOS.
 * TODOS los campos de la siguiente estructura están en memoria compartida.
 */
struct sys_mbox {
	cbuffer_t* cbuffer; 			/* Buffer circular en el que se almacenan los mensajes */
	pthread_cond_t not_empty;
	pthread_cond_t not_full;		/* Cerrojo para garantizar exclusión mutua */
	pthread_mutex_t *mutex;
};

/*-----------------------------------------------------------------------------------*/

struct sys_mbox* mbox_new(unsigned int max_size) {
	struct sys_mbox *mailbox;

	mailbox = (struct sys_mbox *) malloc(sizeof(struct sys_mbox));

	if (mailbox == NULL ) {
		return NULL ;
	}

	/* Create the data structure */
	if ((mailbox->cbuffer = create_cbuffer_t(max_size)) == NULL ) {
		free(mailbox);
		return NULL ;
	}

	pthread_cond_init(&(mailbox->not_empty), NULL);
	pthread_cond_init(&(mailbox->not_full), NULL);
	mailbox->mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(mailbox->mutex, NULL );

	//mailbox->wait_send = 0;
	return mailbox;
}

/*-----------------------------------------------------------------------------------*/
void mbox_free(struct sys_mbox *mbox) {
	if ((mbox != NULL )) {

		pthread_cond_destroy(&(mbox->not_empty));
		pthread_cond_destroy(&(mbox->not_full));

		pthread_mutex_destroy(mbox->mutex);
		free(mbox->mutex);
		destroy_cbuffer_t(mbox->cbuffer);

	//	mbox->not_empty = mbox->not_full = NULL;
		mbox->mutex = NULL;
		mbox->cbuffer = NULL;

		free(mbox);

	}
}


/** mbox_post (equivalente a un productor)
 ** Debe escribir el mensaje "msg" en mbox->cbuffer siempre que haya espacio
 ** Si el buffer está lleno, se incrementerá wait_send (para hacer saber que hay hilos esperando),
 ** y el hilo se quedará bloqueado hasta que el hilo productor invoque a mbox_fetch().
 ** Tras salir del bloqueo, se decrementerá wait_send y se volverá a comprobar si hay espacio (bucle).
 ** Una vez haya espacio, se debe insertar el mensaje en el buffer
 ** En caso de que el buffer estuviera vacío ANTES de esa escritura (es decir,
 ** si este post supone introducir el ÚNICO elemento en el mailbox)
 ** se "informará" a potenciales hilos que estuvieran esperando en un "mbox_fetch()" a que hubiera elementos que consumir del buffer
 **/
/*-----------------------------------------------------------------------------------*/
void mbox_post( struct sys_mbox *mbox, void *msg)
{

	pthread_mutex_lock(mbox->mutex);


	while(is_full_cbuffer_t(mbox->cbuffer)) {



		pthread_cond_wait(&(mbox->not_full),mbox->mutex);



	}

	//int unico_elemento = size_cbuffer_t(mbox->cbuffer);
	insert_cbuffer_t(mbox->cbuffer, msg);

	//if (!unico_elemento) {
		
		pthread_cond_signal(&(mbox->not_empty));
	//}

	pthread_mutex_unlock(mbox->mutex);
}

/*-----------------------------------------------------------------------------------*/
/** mbox_fetch --> cliente
 ** Codificación simétrica a la de post
 ** El hilo invocador comprobará si hay elementos en el buffer circular. Si es así se quedará bloqueado hasta que haya elementos.
 ** A continuación extraerá el primer mensaje del buffer. Una vez, hecho esto, deberá comprobarse si
 ** hay hilos esperando en "post" (es decir, si wait_send es mayor de 0)
 ** y, en tal caso, despertar a uno de esos hilos.
 **/

void* mbox_fetch(struct sys_mbox *mbox)
{

	pthread_mutex_lock(mbox->mutex);


	while(is_empty_cbuffer_t(mbox->cbuffer)) {
		

		pthread_cond_wait(&(mbox->not_empty),mbox->mutex);

	}



	void *msg = head_cbuffer_t(mbox->cbuffer);
	remove_cbuffer_t(mbox->cbuffer);

	//if (mbox->wait_send > 0) {

		pthread_cond_signal(&(mbox->not_full));
	//}

	pthread_mutex_unlock(mbox->mutex);

 return msg;
}

/*-----------------------------------------------------------------------------------*/
