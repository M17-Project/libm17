#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unity/unity.h>
#include <m17.h>

//this is run before every test
void setUp(void)
{
	return;
} 

//this is run after every test
void tearDown(void)
{
	return;
}

void soft_logic_xor(void)
{
    TEST_ASSERT_EQUAL(0x0000, soft_bit_XOR(0x0000, 0x0000));
    TEST_ASSERT_EQUAL(0x7FFE, soft_bit_XOR(0x0000, 0x7FFF)); //off by 1 is acceptable
    TEST_ASSERT_EQUAL(0xFFFE, soft_bit_XOR(0x0000, 0xFFFF)); //off by 1 is acceptable

    TEST_ASSERT_EQUAL(0x7FFE, soft_bit_XOR(0x7FFF, 0x0000)); //off by 1 is acceptable
    TEST_ASSERT_EQUAL(0x7FFE, soft_bit_XOR(0x7FFF, 0x7FFF));
    TEST_ASSERT_EQUAL(0x7FFF, soft_bit_XOR(0x7FFF, 0xFFFF));

    TEST_ASSERT_EQUAL(0xFFFE, soft_bit_XOR(0xFFFF, 0x0000)); //off by 1 is acceptable
    TEST_ASSERT_EQUAL(0x7FFF, soft_bit_XOR(0xFFFF, 0x7FFF));
    TEST_ASSERT_EQUAL(0x0000, soft_bit_XOR(0xFFFF, 0xFFFF));
}

void symbol_to_soft_dibit(uint16_t dibit[2], float symb_in)
{
    //bit 0
    if(symb_in>=symbol_list[3])
    {
        dibit[1]=0xFFFF;
    }
    else if(symb_in>=symbol_list[2])
    {
        dibit[1]=-(float)0xFFFF/(symbol_list[3]-symbol_list[2])*symbol_list[2]+symb_in*(float)0xFFFF/(symbol_list[3]-symbol_list[2]);
    }
    else if(symb_in>=symbol_list[1])
    {
        dibit[1]=0x0000;
    }
    else if(symb_in>=symbol_list[0])
    {
        dibit[1]=(float)0xFFFF/(symbol_list[1]-symbol_list[0])*symbol_list[1]-symb_in*(float)0xFFFF/(symbol_list[1]-symbol_list[0]);
    }
    else
    {
        dibit[1]=0xFFFF;
    }

    //bit 1
    if(symb_in>=symbol_list[2])
    {
        dibit[0]=0x0000;
    }
    else if(symb_in>=symbol_list[1])
    {
        dibit[0]=0x7FFF-symb_in*(float)0xFFFF/(symbol_list[2]-symbol_list[1]);
    }
    else
    {
        dibit[0]=0xFFFF;
    }
}

void symbol_to_dibit(void)
{
    uint16_t dibit[2];

    symbol_to_soft_dibit(dibit, +30.0);
    TEST_ASSERT_EQUAL(0x0000, dibit[0]);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[1]); //this is the LSB...

    symbol_to_soft_dibit(dibit, +4.0);
    TEST_ASSERT_EQUAL(0x0000, dibit[0]);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[1]);

    symbol_to_soft_dibit(dibit, +3.0);
    TEST_ASSERT_EQUAL(0x0000, dibit[0]);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[1]);

    symbol_to_soft_dibit(dibit, +2.0);
    TEST_ASSERT_EQUAL(0x0000, dibit[0]);
    TEST_ASSERT_EQUAL(0x7FFF, dibit[1]);

    symbol_to_soft_dibit(dibit, +1.0);
    TEST_ASSERT_EQUAL(0x0000, dibit[0]);
    TEST_ASSERT_EQUAL(0x0000, dibit[1]);

    symbol_to_soft_dibit(dibit, 0.0);
    TEST_ASSERT_EQUAL(0x7FFF, dibit[0]);
    TEST_ASSERT_EQUAL(0x0000, dibit[1]);

    symbol_to_soft_dibit(dibit, -1.0);
    TEST_ASSERT_EQUAL(0xFFFE, dibit[0]); //off by one is acceptable
    TEST_ASSERT_EQUAL(0x0000, dibit[1]);

    symbol_to_soft_dibit(dibit, -2.0);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[0]);
    TEST_ASSERT_EQUAL(0x7FFF, dibit[1]);
    
    symbol_to_soft_dibit(dibit, -3.0);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[0]);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[1]);

    symbol_to_soft_dibit(dibit, -4.0);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[0]);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[1]);

    symbol_to_soft_dibit(dibit, -30.0);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[0]);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[1]);
}

/**
 * @brief Apply errors to a soft-valued 24-bit logic vector.
 * Errors are spread out evenly among num_errs bits.
 * @param vect Input vector.
 * @param start_pos 
 * @param end_pos 
 * @param num_errs Number of bits to apply errors to.
 * @param sum_errs Sum of all errors (total).
 */
void apply_errors(uint16_t vect[24], uint8_t start_pos, uint8_t end_pos, uint8_t num_errs, float sum_errs)
{
    if(end_pos<start_pos)
    {
        printf("ERROR: Invalid bit range.\nExiting.\n");
        exit(1);
    }

    uint8_t bit_pos;
    uint8_t num_bits=end_pos-start_pos+1;

    if(num_errs>num_bits || num_bits>24 || num_errs>24 || sum_errs>num_errs) //too many errors or too wide range
    {
        printf("ERROR: Impossible combination of error value and number of bits.\nExiting.\n");
        exit(1);
    }

    uint16_t val=roundf((float)0xFFFF*sum_errs/num_errs);
    uint32_t err_loc=0;

    for(uint8_t i=0; i<num_errs; i++)
    {
        //assure we didnt select the same bit more than once
        do
        {
            bit_pos=start_pos+(rand()%num_bits);
        }
        while(err_loc&(1<<bit_pos));
        
        vect[bit_pos]^=val; //apply error
        err_loc|=(1<<bit_pos);
    }
}

void golay_encode(void)
{
    uint16_t data=0x0800;

    //single-bit data
    for(uint8_t i=sizeof(encode_matrix)/sizeof(uint16_t)-1; i>0; i--)
    {
        TEST_ASSERT_EQUAL(((uint32_t)data<<12)|encode_matrix[i], golay24_encode(data));
        data>>=1;
    }

    //test data vector
    data=0x0D78;
    TEST_ASSERT_EQUAL(0x0D7880FU, golay24_encode(data));
}

/**
 * @brief Golay soft-decode one known codeword.
 * 
 */
void golay_soft_decode_clean(void)
{
    uint16_t vector[24];

    //clean D78|80F
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
}

void golay_soft_decode_flipped_parity_1(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 1, 1.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_parity_1(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 1, 0.5);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_flipped_parity_2(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 2, 2.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_parity_2(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 2, 1.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_flipped_parity_3(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 3, 3.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_parity_3(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 3, 1.5);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_parity_3_5(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 7, 3.5);
        TEST_ASSERT_NOT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

//4 errors is exactly half the hamming distance, so due to rounding etc., results may vary
//therefore we run 2 tests here to prove that
void golay_soft_decode_flipped_parity_4(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    vector[6]^=0xFFFF;
    vector[7]^=0xFFFF;
    vector[8]^=0xFFFF;
    vector[11]^=0xFFFF;
    TEST_ASSERT_NOT_EQUAL(0x0D78, golay24_sdecode(vector));
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    vector[6]^=0xFFFF;
    vector[7]^=0xFFFF;
    vector[8]^=0xFFFF;
    vector[9]^=0xFFFF;
    TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF; 
}

void golay_soft_decode_erased_parity_5(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 5, 2.5);
        TEST_ASSERT_NOT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_flipped_parity_5(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 5, 5.0);
        TEST_ASSERT_NOT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_flipped_data_1(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 1, 1.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_data_1(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 1, 0.5);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_flipped_data_2(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 2, 2.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_data_2(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 2, 1.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_flipped_data_3(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 3, 3.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_data_3(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 3, 1.5);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_data_3_5(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 7, 3.5);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

//4 errors is exactly half the hamming distance, so due to rounding etc., results may vary
//therefore we run 2 tests here to prove that
void golay_soft_decode_flipped_data_4(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    vector[12]^=0xFFFF;
    vector[13]^=0xFFFF;
    vector[16]^=0xFFFF;
    vector[22]^=0xFFFF;
    TEST_ASSERT_NOT_EQUAL(0x0D78, golay24_sdecode(vector));
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    vector[14]^=0xFFFF;
    vector[16]^=0xFFFF;
    vector[17]^=0xFFFF;
    vector[20]^=0xFFFF;
    TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF; 
}

void golay_soft_decode_corrupt_data_4_5(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    //4.5 errors - should be uncorrectable - WTF?
    apply_errors(vector, 12, 23, 12, 4.5);
    TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;  
}

void golay_soft_decode_erased_data_5(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 5, 2.5);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_flipped_data_5(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 5, 5.0);
        TEST_ASSERT_NOT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

int main(void)
{
    srand(time(NULL));

	UNITY_BEGIN();

    //soft logic arithmetic
    RUN_TEST(soft_logic_xor);

    //symbol to dibit
    RUN_TEST(symbol_to_dibit);

    //soft Golay
    RUN_TEST(golay_encode);
    RUN_TEST(golay_soft_decode_clean);

    RUN_TEST(golay_soft_decode_erased_parity_1);
    RUN_TEST(golay_soft_decode_flipped_parity_1);
    RUN_TEST(golay_soft_decode_erased_parity_2);
    RUN_TEST(golay_soft_decode_flipped_parity_2);
    RUN_TEST(golay_soft_decode_erased_parity_3);
    RUN_TEST(golay_soft_decode_flipped_parity_3);
    RUN_TEST(golay_soft_decode_erased_parity_3_5);
    RUN_TEST(golay_soft_decode_flipped_parity_4);
    RUN_TEST(golay_soft_decode_erased_parity_5);
    RUN_TEST(golay_soft_decode_flipped_parity_5);

    RUN_TEST(golay_soft_decode_erased_data_1);
    RUN_TEST(golay_soft_decode_flipped_data_1);
    RUN_TEST(golay_soft_decode_erased_data_2);
    RUN_TEST(golay_soft_decode_flipped_data_2);
    RUN_TEST(golay_soft_decode_erased_data_3);
    RUN_TEST(golay_soft_decode_flipped_data_3);
    RUN_TEST(golay_soft_decode_erased_data_3_5);
    RUN_TEST(golay_soft_decode_flipped_data_4);
    RUN_TEST(golay_soft_decode_corrupt_data_4_5);
    RUN_TEST(golay_soft_decode_erased_data_5);
    RUN_TEST(golay_soft_decode_flipped_data_5);

    return UNITY_END();
}
