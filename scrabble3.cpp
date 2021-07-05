#include <iostream>
#include <string.h>

using namespace std; 
int max_score(int x, int y, int z, int q){
	int array[4] = {x, y, z, q};
	int temp = 0;
	for(int i=0;i<4;i++){
		if(array[i]>temp)
		temp=array[i];
	}	
	return temp;
}

int main(){
	int player_number, winner; 
	const int player = 4, rounds = 10;
	int score_pl1=0, score_pl2=0, score_pl3=0, score_pl4=0, temp;
	string player1, player2, player3, player4; 

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	cout << "Enter name of player 1: " << endl;
	cin >> player1;
	cout << "Enter name of player 2: " << endl;
	cin >> player2;
	cout << "Enter name of player 3: " << endl;
	cin >> player3;
	cout << "Enter name of player 4: " << endl;
	cin >> player4; 

	//starting the scoring system
	for(int i=1; i<(rounds+1); i++){
		cout << "Turn number: " << i << endl;
		cout << "Enter player 1's word score: " << endl;
	  	cin >> temp;
		score_pl1 += temp;
		cout << "Enter player 2's word score: " << endl;
		cin >> temp;
		score_pl2 += temp;
		cout << "Enter player 3's word score: " << endl;
		cin >> temp;
		score_pl3 += temp;
		cout << "Enter player 4's word score: " << endl;
		cin >> temp;
		score_pl4 += temp;
		cout << "At the end of round " << i  << " the player totals are: " << endl;
		cout << "Player 1: " << player1 << " Score: " << score_pl1 << endl;
		cout << "Player 2: " << player2 << " Score: " << score_pl2 << endl;
		cout << "Player 3: " << player3 << " Score: " << score_pl3 << endl;
		cout << "Player 4: " << player4 << " Score: " << score_pl4 << endl;
	}
	winner = max_score(score_pl1, score_pl2, score_pl3, score_pl4);
	cout << "The largest score is: " << winner << endl;
	if(winner == score_pl1){
		cout << "The WINNER is: " << player1 << " with " << winner << " points!" << endl;
	}else if(winner == score_pl2){
		cout << "The WINNER is: " << player2 <<  " with " << winner << " points!" << endl;
	}else if(winner == score_pl3){
		cout << "The WINNER is: " << player3 <<  " with " << winner << " points!" << endl;
	}else{
		cout << "The WINNER is: " << player4 <<  " with " << winner << " points!" << endl;
	}
	
	return 0;
}


