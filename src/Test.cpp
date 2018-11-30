#include "Arduino.h"

const int DEFLEN = 02;

 class Employee
{
private:
 char employeeName[DEFLEN];
 int idNumber;
 char department[DEFLEN];
 char jobPosition[DEFLEN];

public:
 
 Employee()
 {
   strcpy (employeeName, "Susan Meyers");
   idNumber = 47899;
   strcpy (department, "Accounting");
   strcpy (jobPosition, "Vice President ");
 }

 Employee(char *employ, char *depart, char *job)
 {
   strcpy (employeeName, "Mark Jones");
   idNumber = 39119;
   strcpy (department, depart);
   strcpy (jobPosition, job);
 }

 Employee(char *employ, char *depart, char *job, int id )
 {
   strcpy (employeeName, employ);
   idNumber = id;
   strcpy (department, depart);
   strcpy (jobPosition, job);
 }


 void setDepartment(char *depart)
 { strcpy(department, depart); }

 void setName(char *employ)
 { strcpy (employeeName, employ); }

 void setPosition(char *job)
 { strcpy (jobPosition, job); }

 void setIdNumber(int id)
 { idNumber = id; }

 char *getName()
 { return employeeName; }

 char *getDepartment()
 { return department; }

 char *getJobPosition()
 { return jobPosition; }

 int getIdNumber()
 { return idNumber;}
};
