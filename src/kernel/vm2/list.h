#ifndef _LIST_H
#define _LIST_H
// Simple linked list
#include <stdlib.h>
#include <stdio.h>
#include "error.h"

struct node 
{
	node *next;
};

class list {
	public:
		list(void){nodeCount=0;rock=NULL;} 
		void add (node *newNode)
			{
			newNode->next=rock;
			rock=newNode;
			nodeCount++;
			}
		//int count(void) {error ("list::count: About to return %d\n",nodeCount);return nodeCount;}
		int count(void) {return nodeCount;}
		node *next(void) 
			{
			//dump();
			node *n=rock;
			if (rock) 
				{
				rock=rock->next;
				nodeCount--;
				}
			//dump();
			return n;
			} 
		void remove(node *toNuke)
			{
			//error ("list::remove starting: nuking %x \n",toNuke);
			//dump();	
			if (rock==toNuke)
				{
				rock=rock->next;
				nodeCount--;
				}
			else
				{
				bool done=false;
				for (struct node *cur=rock;!done && cur->next;cur=cur->next)
					if (cur->next==toNuke)
						{
						cur->next=toNuke->next;
						nodeCount--;
						done=true;
						}
				}
			//error ("list::remove ending: \n");
			//dump();	
			}
		void dump(void)
			{
			for (struct node *cur=rock;cur;cur=cur->next)
				{
				error ("list::dump: At %p, next = %p\n",cur,cur->next);
				}
			}
		struct node *rock;
		int nodeCount;
	private:
};
#endif
