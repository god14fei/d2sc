#
#                           D2SC
#             https://github.com/god14fei/d2sc
#
#   BSD LICENSE
#
#   Copyright(c)
#            2018-2019 Huazhong University of Science and Technology
#
#   All rights reserved.
#
#

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# To add new examples, append the directory name to this variable
examples = basic_monitor

clean_examples=$(addprefix clean_,$(examples))

.PHONY: $(examples) $(clean_examples)

all : $(examples)
clean: $(clean_examples)

$(examples):
	cd $@ && $(MAKE)

$(clean_examples):
	cd $(patsubst clean_%,%,$@) && $(MAKE) clean