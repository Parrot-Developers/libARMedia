/**
 * @file ARMEDIA_Error.h
 * @brief libARMedia errors known.
 * @date 29/03/14
 * @author frederic.dhaeyer@parrot.com
 */
#ifndef _ARMEDIA_ERROR_H_
#define _ARMEDIA_ERROR_H_

/**
 * @brief libARMedia errors known.
 */
typedef enum
{
    ARMEDIA_OK = 0, /**< No error */
    ARMEDIA_ERROR = -1000, /**< Unknown generic error */
    ARMEDIA_ERROR_BAD_PARAMETER, /**< Bad parameters */
    ARMEDIA_ERROR_MANAGER = -2000, /**< Unknown manager error */
    ARMEDIA_ERROR_MANAGER_ALREADY_INITIALIZED, /**< Error manager already initialized */
    ARMEDIA_ERROR_MANAGER_NOT_INITIALIZED, /**< Error manager not initialized */
    ARMEDIA_ERROR_ENCAPSULER = -3000, /**< Error encapsuler generic error */
    
} eARMEDIA_ERROR;

/**
 * @brief Gets the error string associated with an eARMEDIA_ERROR
 * @param error The error to describe
 * @return A static string describing the error
 *
 * @note User should NEVER try to modify a returned string
 */
char* ARMEDIA_Error_ToString (eARMEDIA_ERROR error);

#endif /* _ARMEDIA_ERROR_H_ */
