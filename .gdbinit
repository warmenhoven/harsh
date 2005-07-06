define feed_dump
	set $fd=feeds
	while ($fd != 0)
		print *(struct feed *)(((list *)$fd)->data)
		set $fd=((list *)$fd)->next
	end
end
