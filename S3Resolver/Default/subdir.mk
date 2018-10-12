################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../debugCodes.cpp \
../resolver.cpp \
../sql.cpp 

OBJS += \
./debugCodes.o \
./resolver.o \
./sql.o 

CPP_DEPS += \
./debugCodes.d \
./resolver.d \
./sql.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -D__GXX_EXPERIMENTAL_CXX0X__ -I/home/stefaan/usd/build/include/pxr -O2 -g -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


