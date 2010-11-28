Planet Wars Google AI Challenge 2010
====================================

These are my bot implementations. All C++ implementations are based on [my PlanetWars C++ framework](https://github.com/albertz/planet_wars-cpp).

1. simpleRanking*Bot.cc
-----------------------
My very first one. Ranked first place for the first few days. :)

2. astarBot.cc
--------------
Was supposed to do some kind of A* search on a subtree. But either the subtree was still too big or the heuristic sucked.

I thought about extending this to some bidirectional search (which would not be that complicated) but never came to this.

3. feedForwardBot.py
--------------------
Intended to use a few separated feedforward neural nets. But actually it doesn't use any because I didn't got to it. Those functions where I intended to place them are hardcoded now. Still works somehow.

4. simpleMaxBot.py
------------------
Just a fast try-out but performed really well. It puts many possible next actions in a big list, shuffles it randomly, takes those out which are valid and evaluates them. This is repeated a few times. 

This was my final submission. Sadly, it sometimes times out on the server so my rank is quite bad.

[http://ai-contest.com/profile.php?user_id=3897]

- [Albert Zeyer](http://www.az2000.de)

---

Starter package source:

The entire contents of this starter package are released under the Apache
license as is all code related to the Google AI Challenge. See
code.google.com/p/ai-contest/ for more details.

