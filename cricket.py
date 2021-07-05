# LIST OF FEATURES I WANT THIS SCRIPT TO DO

# Get Averages by Year 
# Get Runs and Averages In Wins/Losses
# Get Runs and Average in First/Second Innings
# Get Runs and Average in 20/20 vs Longer format
# Percentage of dismissals Bowled etc
# 50s per innings First/Second, number of innings per 50
# Try and figure out a way to see my Batting and Bowling Average in games where I have played as a Pure Batsman, Pure Bowler, All Rounder
# Averages Per Team
# Get Runs and Average per format per innings with wins and losses 
# Markov model with friends to predict future 10 innings
# Generates a Report with graphs of Runs 
# Segregate the Average for Batting Post 2010 inclusive and pre 2010 inclusive
# Be able to sort matches by highest score etc
# Add the whole Bowling Segment

# LIST OF KNOWN ISSUES + DATE

# 1) DATE: 28/08/2020 The number of dissmissed innings is 143 but the sum of the varying types of dissmissals is only 142, find the one rogue.


# importing the libraries

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# Reading the Data in using Pandas
data = pd.read_csv('zxc.csv')
matchesPlayed = len(data)

# Storing the Column data in Variables

runScored = data['Score']
totalRS = runScored.sum()

# Note that the CSV file had to be edited to make the "DNB = Did not bat" into 0 
# so that it did not fuck with the pandas sum function

# Finding the Number of Innings Played
# Have to go find the number of instances in which DNB appears in the "How Out" section
# and sum this, then subtract the number from the length of the data to find the dismissals
# Then we can calculate the batting Average

# Finding the Mode of Dissmissal
DNB = 0
NO = 0
Bowled = 0
Caught = 0
LBW = 0
Hitwicket = 0
Runout = 0 
Stumped = 0

# Finding the number of innings over 30,50,100
thirty = 0
fifty = 0
hundred = 0

# First Innings vs Second Innings runs
firstRuns = 0
secondRuns = 0
firstDNB = 0
firstNO = 0
firstInnings = 0
secondDNB = 0
secondNO = 0
secondInnings = 0
# Function to calculate the percentage of dissmissal

def calculatePercentage(dissmissal, total):
	percentage = dissmissal/float(total)*100.0
	return percentage

# Iterating through the data set to sort the desired data.

for index, row in data.iterrows():
	# DISSMISSAL SORTING STATEMENTS
	if row['How Out'] == 'DNB':
		DNB += 1
	if row['How Out'] == 'Not Out':
		NO += 1
	if row['How Out'] == 'Bowled':
		Bowled += 1
	if row['How Out'] == 'LBW':
		LBW += 1
	if row['How Out'] == 'Run Out':
		Runout += 1
	if row['How Out'] == 'Caught':
		Caught += 1	
	if row['How Out'] == 'Hit Wicket':
		Hitwicket += 1	
	if row['How Out'] == 'Stumped':
		Stumped += 1
	# COUNTING SCORES OVER CERTAIN VALUE
	if row['Score'] >= 30:
		thirty += 1
	if row['Score'] >= 50:
		fifty += 1
	if row['Score'] >= 100:
		hundred += 1
	# SORTING STATS BY INNINGS NUMBER
	if row['Innings'] == '1':
		firstRuns += row['Score']
	if row['Innings'] == '2':
		secondRuns += row['Score']

#print(firstRuns)
#print(secondRuns)		


Innings = matchesPlayed - DNB
battingAverage = totalRS/(1.0*Innings-NO)

# Calculating the percentage of dissmissals
dissmissedInnings = Innings - NO
bowledPercent = calculatePercentage(Bowled, dissmissedInnings)
caughtPercent = calculatePercentage(Caught, dissmissedInnings)
runoutPercent = calculatePercentage(Runout, dissmissedInnings)
lbwPercent = calculatePercentage(LBW, dissmissedInnings)
hwPercent = calculatePercentage(Hitwicket, dissmissedInnings)
stumpedPercent = calculatePercentage(Stumped, dissmissedInnings)

# Debugging Print statements# Debugging Print statements

#print("Percentage of Bowled Dissmissals", bowledPercent)
#print("Percentage of Caught Dissmissals", caughtPercent)
#print("Percentage of LBW Dissmissals", runoutPercent)
#print("Percentage of Run Out Dissmissals", lbwPercent)
#print("Percentage of Hit Wicket Dissmissals", hwPercent)
#print("Percentage of Stumped Dissmissals", stumpedPercent)

#print(dissmissedInnings)
#print(Bowled + Caught + Runout + LBW + Hitwicket + Stumped)
#print(bowledPercent + caughtPercent + runoutPercent + lbwPercent + hwPercent + stumpedPercent)
# Section to find the modes of dismissal as a percentage of total dismissal

print("Matches Played: ", matchesPlayed)
print("Innings: ", Innings)
print("Runs: ", totalRS)
print("Batting Average: ", battingAverage) 
print("Not Outs: ", NO)
print("30s : ", thirty)
print("50s: ", fifty)
print("100s: ", hundred)
