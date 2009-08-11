all: 
	@ls *.char | while read i; do echo -n "$$i: "; ruby ./makechar.rb -format=c $$i; done
