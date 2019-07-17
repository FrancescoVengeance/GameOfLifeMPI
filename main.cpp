#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <mpi.h>
#include<allegro5/allegro.h>
#include<allegro5/allegro_primitives.h>
#define mainThread 0
using namespace std;

void disegna(vector<vector<int>>& mappa)
{
	ALLEGRO_TIMER* timer = al_create_timer(0.5);
	ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
	al_register_event_source(queue, al_get_timer_event_source(timer));
	al_start_timer(timer);
	bool close = false;
	while (!close)
	{
		ALLEGRO_EVENT event;
		al_wait_for_event(queue, &event);
		if (event.type == ALLEGRO_EVENT_TIMER)
		{
			for (unsigned i = 0; i < mappa.size(); i++)
			{
				for (unsigned j = 0; j < mappa[i].size(); j++)
				{
					if (mappa[i][j] == 1)
					{
						al_draw_filled_rectangle(j * 4, i * 4, j * 4 + 4, i * 4 + 4, al_map_rgb(255, 255, 255));
					}
				}
			}
			al_flip_display();
		}
		
		close = true;
	}
	
	al_destroy_timer(timer);
	al_destroy_event_queue(queue);
}

int main(int argc, char* argv[])
{
	srand(time(NULL));

	int numProcessi;
	int rank;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numProcessi);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	float tempoInizio = MPI_Wtime();

	ALLEGRO_DISPLAY* display = nullptr;
	
	if (rank == mainThread)
	{
		al_init();
		al_init_primitives_addon();
		al_install_keyboard();
		al_set_new_display_flags(ALLEGRO_FULLSCREEN);
		display = al_create_display(1920, 1080);
		al_clear_to_color(al_map_rgb(0, 0, 0));
	}
	
	int righe = 0;
	int colonne = 0;
	unsigned steps = 0;
	unsigned generazione = 0;
	//double popolazione = 0;

	if (rank == mainThread)
	{
		if (argc != 4)
		{
			cerr << "devi inserire almeno 4 comandi nella shell"<< endl;
			exit(1);
		}

		righe = atoi(argv[1]);
		colonne = atoi(argv[2]);
		steps = atoi(argv[3]);
		//popolazione = atoi(argv[4]);

		if (colonne < 1 || righe < 1 || steps < 1)
		{
			cerr << "numero di colonne, righe o steps invalido" << endl;
			exit(1);
		}
	}

	MPI_Bcast(&righe, 1, MPI_INT, mainThread, MPI_COMM_WORLD);
	MPI_Bcast(&colonne, 1, MPI_INT, mainThread, MPI_COMM_WORLD);
	MPI_Bcast(&steps, 1, MPI_INT, mainThread, MPI_COMM_WORLD);
	
	int righeLocali = righe / numProcessi; // numero di righe locali ad ogni thread
	if (rank == numProcessi - 1) //assegno le righe rimanenti all'ultimo thread
	{
		righeLocali += righe % numProcessi;
	}

	int righeLocaliAggiuntive = righeLocali + 2;  //righe aggiuntive per ogni thread che utilizzo per rendere la matrice toroidale
	int colonneLocaliAggiuntive = colonne + 2; //stessa cosa di sopra ma con le colonne

	vector<vector<int>> generazioneAttuale(righeLocaliAggiuntive,vector<int>(colonneLocaliAggiuntive,0));
	vector<vector<int>> prossimaGenerazione(righeLocaliAggiuntive, vector<int>(colonneLocaliAggiuntive, 0));
	
	/*for (double i = 0; i < righeLocali * righeLocali * popolazione; i++)
	{
		generazioneAttuale[rand() % righeLocali + 1][rand() % colonne + 1] = rand() % 2;
	}*/

	for (unsigned i = 1; i <= righeLocali; i++) //parto da 1 per skippare le righe aggiuntive
	{
		for (unsigned j = 1; j <= colonne; j++)
		{
			generazioneAttuale[i][j] = rand() % 2;
		}
	}

	int cellaDiSopra = (rank == 0) ? numProcessi - 1 : rank - 1;
	int cellaDiSotto = (rank == numProcessi - 1) ? 0 : rank + 1;

	const int cellaViva = 1;
	const int cellaMorta = 0;


	for (unsigned passi = 0; passi < steps; passi++)//loop delle generazioni
	{
		//invio della riga superiore dall'alto
		MPI_Send(&generazioneAttuale[1][0], colonneLocaliAggiuntive, MPI_INT, cellaDiSopra, 0, MPI_COMM_WORLD);
		//invio della riga inferiore dal basso
		MPI_Send(&generazioneAttuale[righeLocali][0], colonneLocaliAggiuntive, MPI_INT, cellaDiSotto, 0, MPI_COMM_WORLD);

		//ricezione della riga inferiore dal basso
		MPI_Status status;
		MPI_Recv(&generazioneAttuale[righeLocali + 1][0], colonneLocaliAggiuntive, MPI_INT, cellaDiSotto, 0, MPI_COMM_WORLD, &status);
		//ricezione della riga superiore dall'alto
		MPI_Recv(&generazioneAttuale[0][0], colonneLocaliAggiuntive, MPI_INT, cellaDiSopra, 0, MPI_COMM_WORLD, &status);

		//riempio le colonne aggiuntive laterali, superiore e inferiore
		for (unsigned j = 0; j < righeLocaliAggiuntive; j++)
		{
			generazioneAttuale[j][0] = generazioneAttuale[j][colonne];
			generazioneAttuale[j][colonne + 1] = generazioneAttuale[j][1];
		}

		//stampa generazione attuale
		if (rank != mainThread)
		{
			for (unsigned i = 1; i <= righeLocali; i++)
			{
				MPI_Send(&generazioneAttuale[i][1], colonne, MPI_INT, mainThread, 0, MPI_COMM_WORLD);
			}
		}
		if (rank == mainThread)
		{
			al_clear_to_color(al_map_rgb(0, 0, 0));
			cout << "Generazione " << generazione << "-------------------------" << endl;
			generazione++;
			vector<vector<int>> mappa;
			for (unsigned i = 1; i <= righeLocali; i++)
			{
				vector<int> temp;
				for (unsigned j = 1; j <= colonne; j++)
				{
					temp.push_back(generazioneAttuale[i][j]);
					cout << generazioneAttuale[i][j] << " ";
				}
				cout << endl;
				mappa.push_back(temp);
			}
			
			for (unsigned rankAttuale = 1; rankAttuale < numProcessi; rankAttuale++)
			{
				int recv = righe / numProcessi;
				if (rankAttuale == numProcessi - 1) recv += righe % numProcessi;
				vector<int> buffer(colonne, 0);
				for (unsigned ricevuti = 0; ricevuti < recv; ricevuti++)
				{
					MPI_Recv(&buffer[0], colonne, MPI_INT, rankAttuale, 0, MPI_COMM_WORLD, &status);
					mappa.push_back(buffer);
					for (unsigned i = 0; i < buffer.size(); i++)
					{
						cout << buffer[i] << " ";
					}
					cout << endl;
				}
			}

			disegna(mappa);
		}

		//aggiornamento generazione
		for (unsigned i = 1; i <= righeLocali; i++)
		{
			for (unsigned j = 1; j < colonne; j++)
			{
				int celleViveVicine = 0;
				for (unsigned k = i - 1; k <= i + 1; k++)
				{
					for (unsigned l = j - 1; l <= j + 1; l++)
					{
						if ((k != i || l != j) && generazioneAttuale[k][l] == cellaViva) //evita di contare se stessi
						{
							celleViveVicine++;
						}
					}
				}
				if (celleViveVicine < 2) prossimaGenerazione[i][j] = cellaMorta;
				if (generazioneAttuale[i][j] == cellaViva && (celleViveVicine == 2 || celleViveVicine == 3)) prossimaGenerazione[i][j] = cellaViva;
				if (celleViveVicine > 3) prossimaGenerazione[i][j] = cellaMorta;
				if (generazioneAttuale[i][j] == cellaMorta && celleViveVicine == 3) prossimaGenerazione[i][j] = cellaViva;
			}
		}

		for (unsigned i = 1; i <= righeLocali; i++)
		{
			for (unsigned j = 1; j <= colonne; j++)
			{
				generazioneAttuale[i][j] = prossimaGenerazione[i][j];
			}
		}
	}
	cout << MPI_Wtime() - tempoInizio << endl;
	MPI_Finalize();
	
	if (rank == mainThread && display != nullptr)
	{
		bool close = false;
		ALLEGRO_KEYBOARD_STATE keyState;
		while (!close)
		{
			al_get_keyboard_state(&keyState);
			if (al_key_down(&keyState, ALLEGRO_KEY_ESCAPE)) close = true;
		}
		al_destroy_display(display);
	}
	return 0;
}