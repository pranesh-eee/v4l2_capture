all: app

obj/%.o:%.c
	@mkdir -p bin lib obj
	@echo "  CC $<"
	$(Q)$(CC) -c -o $@ $(CFLAGS) $<

obj/%.o:%.cpp
	@mkdir -p bin lib obj
	@echo "  CXX $<"
	$(Q)$(CXX) -c -o $@ $(CFLAGS) $<

clean:
	$(Q)$(RM) -fr obj/* lib/* bin/*
	$(Q)$(RM) -fr obj lib bin
	$(Q)$(RM) -fr *.jpg *.data *.yuv

-include obj/*.d
