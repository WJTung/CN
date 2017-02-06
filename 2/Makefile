all : agent.c sender.c receiver.c protocol.h
	gcc agent.c -w -o agent
	gcc sender.c -w -o sender
	gcc receiver.c -w -o receiver
bonus : agent_bonus.c sender_bonus.c receiver_bonus.c protocol.h
	gcc agent_bonus.c -lpthread -w -o agent
	gcc sender_bonus.c -lpthread -w -o sender
	gcc receiver_bonus.c -w -o receiver
