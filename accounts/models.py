from django.db import models
from django.contrib.auth.models import User
# Create your models here.

class File(models.Model):
    image = models.FileField(upload_to='images/')
    user = models.ForeignKey(User, on_delete=models.CASCADE, related_name='files')
    
