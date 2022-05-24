my_id = 9999

function set_object_id(id)
	my_id = id
end

function event_player_move( player_id )
	player_x = API_get_x(player_id)
	player_y = API_get_y(player_id)
	my_x = API_get_x(my_id)
	my_y = API_get_y(my_id)
	if (player_x == my_x) then
		if (player_y == my_y) then
			API_chat(player_id, my_id, "Hello")
		end
	end
end