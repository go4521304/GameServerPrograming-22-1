my_id = 9999
target_id = 9999
moveCount = -1

function set_object_id (id)
my_id = id
end

function event_player_move(player_id)
	player_x = API_get_x(player_id)
	player_y = API_get_y(player_id)
	my_x = API_get_x(my_id)
	my_y = API_get_y(my_id)
	if (player_x == my_x) then
		if (player_y == my_y) then
			API_chat(player_id, my_id, "HELLO");
			moveCount = 3
			target_id = player_id
		end
	end
end

function event_npc_check()
	if (moveCount ~= -1) then
		moveCount = moveCount - 1
		if (moveCount == 0) then
			API_chat(target_id, my_id, "Bye");
			moveCount = -1
			target_id = 9999
		end
	end
end